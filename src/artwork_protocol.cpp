#include "stdafx.h"
// PCH ^

#include <atomic>
#include <algorithm>
#include <limits>
#include <memory>
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
      : items(p_items), art_data(), loaded(false), id(newid) {
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Artwork request " << newid;
    }
  };
  metadb_handle_list items;
  album_art_data_ptr art_data = NULL;
  bool loaded;
  intptr_t id;
};

struct artwork_stream {
  artwork_stream(album_art_data_ptr p_data, intptr_t p_request_id)
      : data(std::move(p_data)), request_id(p_request_id) {}

  album_art_data_ptr data;
  uint64_t cursor = 0;
  intptr_t request_id;
  std::mutex mutex;
};

static std::unique_ptr<artwork_request> g_request;
static std::mutex mutex;
static std::unique_ptr<abort_callback_impl> abort_loading;
static std::thread artwork_loader;
static std::condition_variable cv;
static std::atomic_bool artwork_loader_terminate = false;

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
  std::lock_guard<std::mutex> lock(mutex);
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
  try {
    const auto* stream = static_cast<artwork_stream*>(cookie);
    if (stream == nullptr || stream->data.is_empty())
      return libmpv::MPV_ERROR_GENERIC;

    const auto size = stream->data->get_size();
    if (size > static_cast<t_size>((std::numeric_limits<int64_t>::max)()))
      return libmpv::MPV_ERROR_UNSUPPORTED;
    return static_cast<int64_t>(size);
  } catch (...) {
    return libmpv::MPV_ERROR_GENERIC;
  }
}

static int64_t artworkreader_read(void* cookie, char* buf, uint64_t nbytes) {
  try {
    auto* stream = static_cast<artwork_stream*>(cookie);
    if (stream == nullptr || stream->data.is_empty() ||
        (buf == nullptr && nbytes != 0)) {
      return libmpv::MPV_ERROR_GENERIC;
    }

    std::lock_guard<std::mutex> lock(stream->mutex);
    const uint64_t size = stream->data->get_size();
    if (stream->cursor > size) return libmpv::MPV_ERROR_GENERIC;

    const uint64_t to_read = (std::min)(
        {size - stream->cursor, nbytes,
         static_cast<uint64_t>((std::numeric_limits<size_t>::max)()),
         static_cast<uint64_t>((std::numeric_limits<int64_t>::max)())});
    if (to_read != 0) {
      memcpy(buf, static_cast<const BYTE*>(stream->data->get_ptr()) +
                      static_cast<size_t>(stream->cursor),
             static_cast<size_t>(to_read));
      stream->cursor += to_read;
    }
    return static_cast<int64_t>(to_read);
  } catch (...) {
    return libmpv::MPV_ERROR_GENERIC;
  }
}

static int64_t artworkreader_seek(void* cookie, int64_t offset) {
  try {
    auto* stream = static_cast<artwork_stream*>(cookie);
    if (stream == nullptr || stream->data.is_empty())
      return libmpv::MPV_ERROR_GENERIC;

    std::lock_guard<std::mutex> lock(stream->mutex);
    if (offset < 0 ||
        static_cast<uint64_t>(offset) > stream->data->get_size()) {
      return libmpv::MPV_ERROR_UNSUPPORTED;
    }
    stream->cursor = static_cast<uint64_t>(offset);
    return offset;
  } catch (...) {
    return libmpv::MPV_ERROR_GENERIC;
  }
}

static void artworkreader_close(void* cookie) {
  try {
    delete static_cast<artwork_stream*>(cookie);
  } catch (...) {
    // Never allow C++ exceptions to cross the libmpv callback boundary.
  }
}

int artwork_protocol_open(void* user_data, char* uri,
                          libmpv::mpv_stream_cb_info* info) {
  if (info == nullptr) return libmpv::MPV_ERROR_INVALID_PARAMETER;

  try {
    album_art_data_ptr data;
    intptr_t request_id;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!g_request || g_request->art_data.is_empty()) {
        if (cfg_logging) {
          FB2K_console_formatter() << "mpv: Can't open artwork - missing";
        }
        return libmpv::MPV_ERROR_NOTHING_TO_PLAY;
      }
      data = g_request->art_data;
      request_id = g_request->id;
    }

    auto stream =
        std::make_unique<artwork_stream>(std::move(data), request_id);
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Opening artwork stream [" << request_id << "]";
    }

    info->cookie = stream.release();
    info->close_fn = artworkreader_close;
    info->size_fn = artworkreader_size;
    info->read_fn = artworkreader_read;
    info->seek_fn = artworkreader_seek;
    info->cancel_fn = nullptr;
    return 0;
  } catch (const std::bad_alloc&) {
    return libmpv::MPV_ERROR_NOMEM;
  } catch (...) {
    return libmpv::MPV_ERROR_GENERIC;
  }
}
}  // namespace mpv
