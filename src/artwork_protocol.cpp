#include "stdafx.h"
// PCH ^

#include <atomic>
#include <mutex>

#include "artwork_protocol.h"
#include "libmpv.h"
#include "mpv_container.h"

namespace mpv {
static std::mutex mutex;

static metadb_handle_ptr item = NULL;
static album_art_data_ptr art_data = NULL;
static size_t cursor = 0;
static long request = 0;

extern cfg_uint cfg_artwork_type;
extern cfg_bool cfg_artwork;

abort_callback_impl abort_loading;

static std::thread artwork_loader;
static std::condition_variable cv;
static std::atomic_bool artwork_loader_terminate = false;
static std::atomic_bool load;

bool artwork_loaded() { return art_data.is_valid(); }

void request_artwork(metadb_handle_ptr p_item) {
  if (!cfg_artwork) return;

  {
    std::lock_guard<std::mutex> lock(mutex);
    abort_loading.abort();
    request++;
    item = p_item;
    art_data.reset();
    cursor = 0;
    load = true;
  }

  cv.notify_all();
}

class artwork_register : public initquit {
 public:
  void on_init() override {
    artwork_loader = std::thread([this]() {
      while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] {
          return artwork_loader_terminate ||
                 (load && art_data.is_empty() && item.is_valid());
        });
        abort_loading.reset();
        metadb_handle_ptr request_item = item;
        unsigned request_number = request;
        load = false;
        lock.unlock();

        if (artwork_loader_terminate) return;

        album_art_data_ptr result;
        try {
          pfc::list_t<GUID> types;
          GUID type;
          switch (cfg_artwork_type) {
            case 0:
              type = album_art_ids::cover_front;
              break;
            case 1:
              type = album_art_ids::cover_back;
              break;
            case 2:
              type = album_art_ids::disc;
              break;
            case 3:
              type = album_art_ids::artist;
              break;
            default:
              uBugCheck();
          }

          types.add_item(type);

          metadb_handle_list list;
          list.add_item(request_item);
          album_art_extractor_instance::ptr extractor =
              album_art_manager_v2::get()->open(list, types, abort_loading);
          result = extractor->query(type, abort_loading);
          {
            std::lock_guard<std::mutex> lock(mutex);
            if (request_number == request) {
              art_data = result;
              mpv_on_new_artwork();
            }
          }
        } catch (...) {
          if (request_number == request) {
            mpv_on_new_artwork();
          }
        }
      }
    });
  }

  void on_quit() override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      artwork_loader_terminate = true;
    }
    cv.notify_all();

    if (artwork_loader.joinable()) {
      artwork_loader.join();
    }

    std::lock_guard<std::mutex> lock2(mutex);
    request++;
    art_data.reset();
  }
};

static initquit_factory_t<artwork_register> g_np_register;

static int64_t artworkreader_size(void* cookie) {
  std::lock_guard<std::mutex> lock(mutex);
  if ((long)cookie != request || art_data.is_empty()) {
    return MPV_ERROR_GENERIC;
  }
  return art_data->get_size();
}

static int64_t artworkreader_read(void* cookie, char* buf, uint64_t nbytes) {
  std::lock_guard<std::mutex> lock(mutex);
  if ((long)cookie != request || art_data.is_empty()) {
    return -1;
  }
  t_size to_read =
      (t_size)min((uint64_t)art_data->get_size() - (uint64_t)cursor, nbytes);
  memcpy(buf, (BYTE*)art_data->get_ptr() + cursor, to_read);
  cursor += to_read;
  return to_read;
}

static int64_t artworkreader_seek(void* cookie, int64_t offset) {
  std::lock_guard<std::mutex> lock(mutex);
  if ((long)cookie != request || art_data.is_empty()) {
    return MPV_ERROR_GENERIC;
  }
  if (offset < 0 || offset > art_data->get_size()) {
    return MPV_ERROR_UNSUPPORTED;
  }
  cursor = (t_size)offset;
  return cursor;
}

static void artworkreader_close(void* cookie) {}

int artwork_protocol_open(void* user_data, char* uri,
                          mpv_stream_cb_info* info) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (art_data.is_empty()) {
      return MPV_ERROR_NOTHING_TO_PLAY;
    }
    info->cookie = (void*)request;
  }
  info->close_fn = artworkreader_close;
  info->size_fn = artworkreader_size;
  info->read_fn = artworkreader_read;
  info->seek_fn = artworkreader_seek;
  return 0;
}
}  // namespace mpv
