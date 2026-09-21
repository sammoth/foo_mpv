#include "stdafx.h"
// PCH ^

#include <atomic>
#include <mutex>

#include "artwork_protocol.h"
#include "libmpv.h"
#include "mpv_player.h"

namespace mpv {
extern cfg_uint cfg_artwork_type;
extern cfg_bool cfg_artwork;
extern advconfig_checkbox_factory cfg_logging;

struct artwork_request {
  artwork_request(metadb_handle_list_cref p_items, intptr_t newid)
      : items(p_items), art_data(), cursor(0), loaded(false), id(newid) {
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Artwork request " << newid;
    }
  };
  metadb_handle_list items;
  album_art_data_ptr art_data = NULL;
  bool loaded;
  size_t cursor;
  intptr_t id;
};

static std::unique_ptr<artwork_request> g_request;
static std::mutex mutex;
static std::unique_ptr<abort_callback_impl> abort_loading;
static std::thread artwork_loader;
static std::condition_variable cv;
static std::atomic_bool artwork_loader_terminate = false;
static std::atomic_bool load;

static abort_callback_impl& get_abort_loading() {
  if (!abort_loading) {
    abort_loading = std::make_unique<abort_callback_impl>();
  }
  return *abort_loading;
}

bool artwork_loaded() {
  std::lock_guard<std::mutex> lock(mutex);
  bool ret = g_request && g_request->art_data.is_valid();
  if (cfg_logging && ret) {
    FB2K_console_formatter()
        << "mpv: Loading artwork protocol [" << g_request->id << "]";
  }
  return ret;
}

metadb_handle_ptr single_artwork_item() {
  if (g_request && g_request->items.get_count() == 1) {
    return g_request->items[0];
  } else {
    return NULL;
  }
}

void reload_artwork() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (g_request) {
      get_abort_loading().abort();
      intptr_t id = g_request ? g_request->id + 1 : 0;
      g_request.reset(new artwork_request(g_request->items, id));
    }
  }

  cv.notify_all();
}

void request_artwork() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    get_abort_loading().abort();
    metadb_handle_list selection;
    ui_selection_manager::get()->get_selection(selection);
    intptr_t id = g_request ? g_request->id + 1 : 0;
    g_request.reset(new artwork_request(selection, id));
  }

  cv.notify_all();
}

void request_artwork(metadb_handle_list_cref p_items) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    get_abort_loading().abort();
    intptr_t id = g_request ? g_request->id + 1 : 0;
    g_request.reset(new artwork_request(p_items, id));
  }

  cv.notify_all();
}

class artwork_register : public initquit {
 public:
  void on_init() override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      get_abort_loading();
    }
    artwork_loader = std::thread([this]() {
      while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] {
          return artwork_loader_terminate ||
                 get_abort_loading().is_aborting() ||
                 (g_request && !g_request->loaded);
        });
        if (artwork_loader_terminate) {
          lock.unlock();
          return;
        }

        if (get_abort_loading().is_aborting()) {
          get_abort_loading().reset();
          lock.unlock();
        } else if (!cfg_artwork) {
          g_request->loaded = true;
          lock.unlock();
          mpv_player::on_new_artwork();
        } else {
          metadb_handle_list req_items(g_request->items);
          intptr_t req_id = g_request->id;
          lock.unlock();

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

            try {
              album_art_extractor_instance::ptr extractor =
                  album_art_manager_v2::get()->open(req_items, types,
                                                    *abort_loading);
              result = extractor->query(type, *abort_loading);
            } catch (exception_album_art_not_found e) {
              album_art_extractor_instance::ptr extractor =
                  album_art_manager_v2::get()->open_stub(*abort_loading);
              result = extractor->query(type, *abort_loading);
            }

            {
              std::lock_guard<std::mutex> lock(mutex);
              if (g_request && g_request->id == req_id) {
                g_request->art_data = result;
                g_request->loaded = true;
                mpv_player::on_new_artwork();
              }
            }
          } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            if (g_request && g_request->id == req_id) {
              g_request->loaded = true;
              mpv_player::on_new_artwork();
            }
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
    g_request.reset();
    abort_loading.reset();
  }
};

static initquit_factory_t<artwork_register> g_np_register;

static int64_t artworkreader_size(void* cookie) {
  std::lock_guard<std::mutex> lock(mutex);
  const intptr_t request_id = reinterpret_cast<intptr_t>(cookie);
  if (!g_request || request_id != g_request->id) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Stale artwork reference [size, " << request_id << "]";
    }
    return libmpv::MPV_ERROR_GENERIC;
  }
  if (g_request->art_data.is_empty()) {
    return 0;
  }
  return g_request->art_data->get_size();
}

static int64_t artworkreader_read(void* cookie, char* buf, uint64_t nbytes) {
  std::lock_guard<std::mutex> lock(mutex);
  const intptr_t request_id = reinterpret_cast<intptr_t>(cookie);
  if (!g_request || request_id != g_request->id) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Stale artwork reference [read, " << request_id << "]";
    }
    return libmpv::MPV_ERROR_GENERIC;
  }
  if (g_request->art_data.is_empty()) {
    return libmpv::MPV_ERROR_GENERIC;
  }
  t_size to_read = (t_size)min(
      (uint64_t)g_request->art_data->get_size() - (uint64_t)g_request->cursor,
      nbytes);
  memcpy(buf, (BYTE*)g_request->art_data->get_ptr() + g_request->cursor,
         to_read);
  g_request->cursor += to_read;
  return to_read;
}

static int64_t artworkreader_seek(void* cookie, int64_t offset) {
  std::lock_guard<std::mutex> lock(mutex);
  const intptr_t request_id = reinterpret_cast<intptr_t>(cookie);
  if (!g_request || request_id != g_request->id) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Stale artwork reference [size, " << request_id << "]";
    }
    return libmpv::MPV_ERROR_GENERIC;
  }
  if (g_request->art_data.is_empty()) {
    return libmpv::MPV_ERROR_GENERIC;
  }
  if (offset < 0 || static_cast<uint64_t>(offset) >
                        g_request->art_data->get_size()) {
    return libmpv::MPV_ERROR_UNSUPPORTED;
  }
  g_request->cursor = (t_size)offset;
  return g_request->cursor;
}

static void artworkreader_close(void* cookie) {}

int artwork_protocol_open(void* user_data, char* uri,
                          libmpv::mpv_stream_cb_info* info) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!g_request || g_request->art_data.is_empty()) {
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Can't open artwork - missing";
      }
      return libmpv::MPV_ERROR_NOTHING_TO_PLAY;
    }
    info->cookie = reinterpret_cast<void*>(g_request->id);
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Opening artwork stream [" << g_request->id << "]";
    }
  }
  info->close_fn = artworkreader_close;
  info->size_fn = artworkreader_size;
  info->read_fn = artworkreader_read;
  info->seek_fn = artworkreader_seek;
  return 0;
}
}  // namespace mpv
