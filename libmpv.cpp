#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <mutex>
#include <set>
#include <thread>

#include "helpers/atl-misc.h"
#include "helpers/win32_misc.h"
#include "libmpv.h"

namespace mpv {

static const GUID guid_cfg_mpv_branch = {
    0xa8d3b2ca,
    0xa9a,
    0x4efc,
    {0xa4, 0x33, 0x32, 0x4d, 0x76, 0xcc, 0x8a, 0x33}};
static const GUID guid_cfg_mpv_max_drift = {
    0xa799d117,
    0x7e68,
    0x4d1e,
    {0x9d, 0xc2, 0xe8, 0x16, 0x1e, 0xf1, 0xf5, 0xfe}};
static const GUID guid_cfg_mpv_hard_sync = {
    0x240d9ab0,
    0xb58d,
    0x4565,
    {0x9e, 0xc0, 0x6b, 0x27, 0x99, 0xcd, 0x2d, 0xed}};
static const GUID guid_cfg_mpv_hard_sync_interval = {
    0x79009e9f,
    0x11f0,
    0x4518,
    {0x9e, 0x75, 0xe1, 0x26, 0x99, 0xa8, 0x65, 0xcb}};

static const GUID guid_cfg_mpv_logging = {
    0x8b74d741,
    0x232a,
    0x46d5,
    {0xa7, 0xee, 0x4, 0x89, 0xb1, 0x47, 0x43, 0xf0}};
static const GUID guid_cfg_mpv_native_logging = {
    0x3411741c,
    0x239,
    0x441d,
    {0x8a, 0x8e, 0x99, 0x83, 0x2a, 0xda, 0xe7, 0xd0}};
static const GUID guid_cfg_mpv_stop_hidden = {
    0x9de7e631,
    0x64f8,
    0x4047,
    {0x88, 0x39, 0x8f, 0x4a, 0x50, 0xa0, 0xb7, 0x2f}};

static advconfig_branch_factory g_mpv_branch(
    "mpv", guid_cfg_mpv_branch, advconfig_branch::guid_branch_playback, 0);
static advconfig_integer_factory cfg_mpv_max_drift(
    "Permitted timing drift (ms)", guid_cfg_mpv_max_drift, guid_cfg_mpv_branch,
    0, 20, 0, 1000, 0);
static advconfig_integer_factory cfg_mpv_hard_sync("Hard sync threshold (ms)",
                                                   guid_cfg_mpv_hard_sync,
                                                   guid_cfg_mpv_branch, 0, 800,
                                                   0, 10000, 0);
static advconfig_integer_factory cfg_mpv_hard_sync_interval(
    "Minimum time between hard syncs (seconds)",
    guid_cfg_mpv_hard_sync_interval, guid_cfg_mpv_branch, 0, 5, 0, 30, 0);
static advconfig_checkbox_factory cfg_mpv_logging(
    "Enable verbose console logging", guid_cfg_mpv_logging, guid_cfg_mpv_branch,
    0, false);
static advconfig_checkbox_factory cfg_mpv_native_logging(
    "Enable mpv log file", guid_cfg_mpv_native_logging, guid_cfg_mpv_branch, 0,
    false);
static advconfig_checkbox_factory cfg_mpv_stop_hidden("Stop when hidden",
                                                      guid_cfg_mpv_stop_hidden,
                                                      guid_cfg_mpv_branch, 0,
                                                      true);

static std::vector<mpv_container*> mpv_containers;
static std::unique_ptr<CMpvWindow> mpv_window;

mpv_container* mpv_container::get_main() {
  std::sort(mpv_containers.begin(), mpv_containers.end(),
            [](mpv_container* a, mpv_container* b) {
              if (a->is_pinned()) return true;
              if (b->is_pinned()) return false;
              if (!a->is_visible()) return false;
              if (!b->is_visible()) return true;
              return a->priority() > b->priority();
            });

  return *mpv_containers.begin();
}

bool mpv_container::is_on() {
  return mpv_window != NULL && mpv_window->get_container() == this;
}

void mpv_container::resize(long p_x, long p_y) {
  x = p_x;
  y = p_y;
  if (mpv_window) {
    mpv_window->update();
  }
}

void mpv_container::update() {
  if (mpv_window) {
    mpv_window->update();
  }
}

bool mpv_container::is_pinned() { return pinned; }

void mpv_container::unpin() {
  for (auto it = mpv_containers.begin(); it != mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  if (mpv_window) {
    mpv_window->update();
  }
}

void mpv_container::pin() {
  for (auto it = mpv_containers.begin(); it != mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  pinned = true;

  if (mpv_window) {
    mpv_window->update();
  }
}

void mpv_container::create() {
  mpv_containers.push_back(this);

  if (!mpv_window) {
    mpv_window = std::unique_ptr<CMpvWindow>(new CMpvWindow());
  } else {
    mpv_window->update();
  }
}

void mpv_container::destroy() {
  mpv_containers.erase(
      std::remove(mpv_containers.begin(), mpv_containers.end(), this),
      mpv_containers.end());

  if (mpv_containers.empty()) {
    if (mpv_window != NULL) mpv_window->DestroyWindow();
    mpv_window = NULL;
  }

  if (mpv_window) {
    mpv_window->update();
  }
}

mpv_player::mpv_player()
    : wid(NULL),
      enabled(false),
      mpv(NULL),
      sync_task(sync_task_type::Wait),
      sync_on_unpause(false),
      last_mpv_seek(0),
      disabled(false),
      time_base(0) {
  mpv_loaded = load_mpv();
  sync_thread = std::thread([this]() {
    while (true) {
      std::unique_lock<std::mutex> lock(cv_mutex);
      cv.wait(lock, [this] { return sync_task != sync_task_type::Wait; });
      if (sync_task == sync_task_type::Stop) {
        sync_task = sync_task_type::Wait;
        lock.unlock();
        cv.notify_all();
      } else {
        sync_task_type task = sync_task;
        lock.unlock();
        switch (task) {
          case sync_task_type::Quit:
            return;
          case sync_task_type::FirstFrameSync:
            mpv_first_frame_sync();
            sync_task = sync_task_type::Stop;
            break;
          default:
            break;
        }
      }
    }
  });
}

mpv_player::~mpv_player() {
  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Quit;
  }
  cv.notify_all();
  sync_thread.join();
}

void mpv_player::mpv_init() {
  if (!mpv_loaded) return;

  if (mpv == NULL && wid != NULL) {
    pfc::string_formatter path;
    path.add_filename(core_api::get_profile_path());
    path.add_filename("mpv");
    path.replace_string("\\file://", "");
    mpv = _mpv_create();

    int64_t l_wid = (intptr_t)(wid);
    _mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &l_wid);

    _mpv_set_option_string(mpv, "load-scripts", "no");
    _mpv_set_option_string(mpv, "ytdl", "no");
    _mpv_set_option_string(mpv, "load-stats-overlay", "no");
    _mpv_set_option_string(mpv, "load-osd-console", "no");

    _mpv_set_option_string(mpv, "config", "yes");
    _mpv_set_option_string(mpv, "config-dir", path.c_str());

    if (cfg_mpv_native_logging.get()) {
      path.add_filename("mpv.log");
      _mpv_set_option_string(mpv, "log-file", path.c_str());
    }

    // no display for music
    _mpv_set_option_string(mpv, "audio-display", "no");

    // everything syncs to foobar
    _mpv_set_option_string(mpv, "video-sync", "audio");
    _mpv_set_option_string(mpv, "untimed", "no");

    // seek fast
    _mpv_set_option_string(mpv, "hr-seek-framedrop", "yes");
    _mpv_set_option_string(mpv, "hr-seek-demuxer-offset", "0");

    // foobar plays the audio
    _mpv_set_option_string(mpv, "audio", "no");

    // start timing immediately to keep in sync
    _mpv_set_option_string(mpv, "no-initial-audio-sync", "yes");

    // keep the renderer initialised
    _mpv_set_option_string(mpv, "force-window", "yes");
    _mpv_set_option_string(mpv, "idle", "yes");

    // don't unload the file when finished, maybe fb is still playing and we
    // could be asked to seek backwards
    _mpv_set_option_string(mpv, "keep-open", "yes");

    if (_mpv_initialize(mpv) != 0) {
      _mpv_destroy(mpv);
      mpv = NULL;
    }
  }
}

void mpv_player::mpv_terminate() {
  if (!mpv_loaded) return;

  mpv_stop();

  if (mpv != NULL) {
    mpv_handle* temp = mpv;
    mpv = NULL;
    _mpv_terminate_destroy(temp);
  }
};

void mpv_player::mpv_update_visibility() {
  if (!mpv_loaded) return;

  if (!disabled && (mpv_is_visible() || !cfg_mpv_stop_hidden.get())) {
    bool starting = !enabled;
    enabled = true;

    if (starting && playback_control::get()->is_playing()) {
      metadb_handle_ptr handle;
      playback_control::get()->get_now_playing(handle);
      mpv_play(handle, false);
    }
  } else {
    bool stopping = enabled;
    enabled = false;

    if (stopping) {
      mpv_stop();
    }
  }
}

struct timing_info {
  double last_fb_seek;
  double last_seek_vistime;
};
static std::atomic<timing_info> g_timing_info;
class timing_info_callbacks : public play_callback_static {
  void on_playback_new_track(metadb_handle_ptr p_track) {
    visualisation_stream::ptr vis_stream;
    visualisation_manager::get()->create_stream(vis_stream, 0);
    double vistime;
    vis_stream->get_absolute_time(vistime);
    double fb_time = playback_control::get()->playback_get_position();
    g_timing_info.store({0.0, vistime - fb_time});
  }

  void on_playback_seek(double p_time) { g_timing_info.store({p_time, 0.0}); }
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused){};
  void on_playback_stop(play_control::t_stop_reason p_reason){};
  void on_playback_pause(bool p_state){};
  void on_playback_time(double p_time){};
  void on_playback_edited(metadb_handle_ptr p_track){};
  void on_playback_dynamic_info(const file_info& p_info){};
  void on_playback_dynamic_info_track(const file_info& p_info){};
  void on_volume_change(float p_new_val){};

  unsigned get_flags() {
    return play_callback::flag_on_playback_new_track |
           play_callback::flag_on_playback_seek;
  }
};
static service_factory_single_t<timing_info_callbacks> g_timing_info_callbacks;

void mpv_player::mpv_set_wid(HWND wnd) { wid = wnd; }

void mpv_player::on_playback_starting(play_control::t_track_command p_command,
                                      bool p_paused) {}
void mpv_player::on_playback_new_track(metadb_handle_ptr p_track) {
  mpv_play(p_track, true);
}
void mpv_player::on_playback_stop(play_control::t_stop_reason p_reason) {
  mpv_stop();
}
void mpv_player::on_playback_seek(double p_time) { mpv_seek(p_time, true); }
void mpv_player::on_playback_pause(bool p_state) { mpv_pause(p_state); }
void mpv_player::on_playback_time(double p_time) {
  mpv_sync(p_time);
  mpv_update_visibility();
}

void mpv_player::mpv_play(metadb_handle_ptr metadb, bool new_file) {
  if (!mpv_loaded) return;

  mpv_update_visibility();

  if (!enabled) return;

  if (mpv == NULL) mpv_init();

  sync_on_unpause = false;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    pfc::string8 filename;
    filename.add_filename(metadb->get_path());
    if (filename.has_prefix("\\file://")) {
      filename.remove_chars(0, 8);

      bool paused = playback_control::get()->is_paused();

      time_base = 0.0;
      if (metadb->get_subsong_index() > 1) {
        for (t_uint32 s = 0; s < metadb->get_subsong_index(); s++) {
          playable_location_impl tmp = metadb->get_location();
          tmp.set_subsong(s);
          metadb_handle_ptr subsong = metadb::get()->handle_create(tmp);
          if (subsong.is_valid()) {
            time_base += subsong->get_length();
          }
        }
      }

      double start_time =
          new_file ? 0.0 : playback_control::get()->playback_get_position();
      last_mpv_seek = ceil(1000 * start_time) / 1000.0;

      std::stringstream time_sstring;
      time_sstring.setf(std::ios::fixed);
      time_sstring.precision(3);
      time_sstring << time_base + start_time;
      std::string time_string = time_sstring.str();
      _mpv_set_option_string(mpv, "start", time_string.c_str());
      if (cfg_mpv_logging.get()) {
        std::stringstream msg;
        msg << "mpv: Loading item '" << filename << "' at start time "
            << time_base + start_time;
        console::info(msg.str().c_str());
      }

      // reset speed
      double unity = 1.0;
      if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &unity) < 0 &&
          cfg_mpv_logging.get()) {
        console::error("mpv: Error setting speed");
      }

      const char* cmd[] = {"loadfile", filename.c_str(), NULL};
      if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get()) {
        std::stringstream msg;
        msg << "mpv: Error loading item '" << filename << "'";
        console::error(msg.str().c_str());
      }

      if (_mpv_set_property_string(mpv, "pause", paused ? "yes" : "no") < 0 &&
          cfg_mpv_logging.get()) {
        console::error("mpv: Error pausing");
      }

      if (paused) {
        sync_on_unpause = true;
      } else {
        sync_task = sync_task_type::FirstFrameSync;
      }
    } else if (cfg_mpv_logging.get()) {
      std::stringstream msg;
      msg << "mpv: Skipping loading item '" << filename
          << "' because it is not a local file";
      console::info(msg.str().c_str());
    }

    lock.unlock();
  }
  cv.notify_all();
}

void mpv_player::mpv_stop() {
  if (!mpv_loaded) return;

  if (mpv == NULL) return;

  sync_on_unpause = false;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    if (_mpv_command_string(mpv, "stop") < 0 && cfg_mpv_logging.get()) {
      console::error("mpv: Error stopping video");
    }

    lock.unlock();
  }
  cv.notify_all();
}

bool mpv_player::check_for_idle() {
  int idle = 0;
  _mpv_get_property(mpv, "idle-active", MPV_FORMAT_FLAG, &idle);

  return idle == 1;
}

void mpv_player::mpv_pause(bool state) {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  if (check_for_idle()) return;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    if (_mpv_set_property_string(mpv, "pause", state ? "yes" : "no") < 0 &&
        cfg_mpv_logging.get()) {
      console::error("mpv: Error pausing");
    }

    if (!state && sync_on_unpause) {
      sync_on_unpause = false;
      sync_task = sync_task_type::FirstFrameSync;
    }

    lock.unlock();
  }
  cv.notify_all();
}

void mpv_player::mpv_seek(double time, bool sync_after) {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  if (check_for_idle()) return;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    if (cfg_mpv_logging.get()) {
      std::stringstream msg;
      msg << "mpv: Seeking to " << time;
      console::info(msg.str().c_str());
    }

    last_mpv_seek = time;
    bool paused = playback_control::get()->is_paused();

    // reset speed
    double unity = 1.0;
    if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &unity) < 0 &&
        cfg_mpv_logging.get()) {
      console::error("mpv: Error setting speed");
    }

    // build command
    std::stringstream time_sstring;
    time_sstring.setf(std::ios::fixed);
    time_sstring.precision(15);
    time_sstring << time_base + time;
    std::string time_string = time_sstring.str();
    const char* cmd[] = {"seek", time_string.c_str(), "absolute+exact", NULL};

    if (_mpv_command(mpv, cmd) < 0) {
      if (cfg_mpv_logging.get()) {
        console::error("mpv: Error seeking, waiting for file to load first");
      }

      const int64_t userdata = 853727396;
      if (_mpv_observe_property(mpv, userdata, "seeking", MPV_FORMAT_FLAG) <
          0) {
        if (cfg_mpv_logging.get()) {
          console::error("mpv: Error observing seeking");
        }
      } else {
        for (int i = 0; i < 100; i++) {
          mpv_event* event = _mpv_wait_event(mpv, 0.05);

          if (check_for_idle()) {
            _mpv_unobserve_property(mpv, userdata);
            return;
          }

          if (event->event_id == MPV_EVENT_SHUTDOWN) {
            _mpv_unobserve_property(mpv, userdata);
            break;
          }

          if (event->event_id == MPV_EVENT_PROPERTY_CHANGE &&
              event->reply_userdata == userdata) {
            mpv_event_property* event_property =
                (mpv_event_property*)event->data;

            if (event_property->format != MPV_FORMAT_FLAG)
              continue;  // no frame decoded yet

            int seeking = *(int*)(event_property->data);
            if (seeking == 0) {
              break;
            }
          }
        }
      }
      _mpv_unobserve_property(mpv, userdata);

      if (_mpv_command(mpv, cmd) < 0) {
        if (cfg_mpv_logging.get()) {
          console::error("mpv: Error seeking");
        }
      }
    }

    if (sync_after) {
      if (paused) {
        sync_on_unpause = true;
      } else {
        sync_task = sync_task_type::FirstFrameSync;
      }
    }

    lock.unlock();
  }
  cv.notify_all();
}

void mpv_player::mpv_sync(double debug_time) {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  if (check_for_idle()) return;

  if (sync_task == sync_task_type::FirstFrameSync) {
    if (cfg_mpv_logging.get()) {
      console::info("mpv: Skipping regular sync");
    }
    return;
  }

  double mpv_time = -1.0;
  if (_mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
    return;

  double fb_time = playback_control::get()->playback_get_position();
  double desync = time_base + fb_time - mpv_time;
  double new_speed = 1.0;

  if (abs(desync) > 0.001 * cfg_mpv_hard_sync.get() &&
      (fb_time - last_mpv_seek) > cfg_mpv_hard_sync_interval.get()) {
    // hard sync
    mpv_seek(fb_time, true);
    if (cfg_mpv_logging.get()) {
      console::info("mpv: A/V sync");
    }
  } else {
    // soft sync
    if (abs(desync) > 0.001 * cfg_mpv_max_drift.get()) {
      // aim to correct mpv internal timer in 1 second, then let mpv catch up
      // the video
      new_speed = min(max(1.0 + desync, 0.01), 100.0);
    }

    if (cfg_mpv_logging.get()) {
      std::stringstream msg;
      msg.setf(std::ios::fixed);
      msg.setf(std::ios::showpos);
      msg.precision(10);
      msg << "mpv: Sync at " << debug_time << " video offset " << desync
          << "; setting mpv speed to " << new_speed;
      console::info(msg.str().c_str());
    }

    if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_mpv_logging.get()) {
      console::error("mpv: Error setting speed");
    }
  }
}

void mpv_player::mpv_first_frame_sync() {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  std::stringstream msg;
  msg.setf(std::ios::fixed);
  msg.setf(std::ios::showpos);
  msg.precision(10);

  if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("mpv: Error pausing");
  }

  if (cfg_mpv_logging.get()) {
    console::info("mpv: Initial sync");
  }

  visualisation_stream::ptr vis_stream = NULL;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  if (!vis_stream.is_valid()) {
    console::error(
        "mpv: Video disabled: this output has no timing information");
    fb2k::inMainThread([this]() {
      disabled = true;
      mpv_update_visibility();
    });
    return;
  }

  const int64_t userdata = 208341047;
  if (_mpv_observe_property(mpv, userdata, "time-pos", MPV_FORMAT_DOUBLE) < 0) {
    if (cfg_mpv_logging.get()) {
      console::error("mpv: Error observing time-pos");
    }
    return;
  }
  if (_mpv_observe_property(mpv, userdata, "pause", MPV_FORMAT_FLAG) < 0) {
    if (cfg_mpv_logging.get()) {
      console::error("mpv: Error observing time-pos");
    }
    return;
  }

  double mpv_time = -1.0;
  while (true) {
    if (sync_task != sync_task_type::FirstFrameSync) {
      _mpv_unobserve_property(mpv, userdata);
      return;
    }

    mpv_event* event = _mpv_wait_event(mpv, 0.01);
    if (check_for_idle()) {
      _mpv_unobserve_property(mpv, userdata);
      return;
    }

    if (event->event_id == MPV_EVENT_SHUTDOWN) {
      _mpv_unobserve_property(mpv, userdata);
      return;
    }

    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE &&
        event->reply_userdata == userdata) {
      mpv_event_property* event_property = (mpv_event_property*)event->data;

      if (strcmp(event_property->name, "pause") == 0 &&
          event_property->format > 0 && *(int*)(event_property->data) == 1) {
        _mpv_unobserve_property(mpv, userdata);
        if (cfg_mpv_logging.get()) {
          console::info("mpv: Initial sync aborted - pause");
        }
        return;  // paused while waiting
      }

      if (event_property->format != MPV_FORMAT_DOUBLE)
        continue;  // no frame decoded yet

      mpv_time = *(double*)(event_property->data);
      if (mpv_time > last_mpv_seek) {
        // frame decoded, wait for fb
        if (cfg_mpv_logging.get()) {
          msg.str("");
          msg << "mpv: First frame found at timestamp " << mpv_time
              << " after seek to " << last_mpv_seek << ", pausing";
          console::info(msg.str().c_str());
        }

        if (sync_task != sync_task_type::FirstFrameSync) {
          _mpv_unobserve_property(mpv, userdata);
          return;
        }

        if (_mpv_set_property_string(mpv, "pause", "yes") < 0 &&
            cfg_mpv_logging.get()) {
          console::error("mpv: Error pausing");
        }

        break;
      }
    }
  }
  _mpv_unobserve_property(mpv, userdata);

  // wait for fb to catch up to the first frame
  double vis_time = -1.0;
  vis_stream->get_absolute_time(vis_time);
  if (vis_time < 0) {
    console::error(
        "mpv: Video disabled: this output has no timing information");

    fb2k::inMainThread([this]() {
      disabled = true;
      mpv_update_visibility();
    });
    return;
  }

  if (cfg_mpv_logging.get()) {
    msg.str("");
    msg << "mpv: Audio time "
        << time_base + g_timing_info.load().last_fb_seek + vis_time -
               g_timing_info.load().last_seek_vistime;
    console::info(msg.str().c_str());
  }

  while (time_base + g_timing_info.load().last_fb_seek + vis_time -
             g_timing_info.load().last_seek_vistime <
         mpv_time) {
    if (sync_task != sync_task_type::FirstFrameSync) {
      return;
    }
    Sleep(10);
    vis_stream->get_absolute_time(vis_time);
  }

  if (sync_task != sync_task_type::FirstFrameSync) {
    return;
  }

  if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("mpv: Error pausing");
  }

  if (cfg_mpv_logging.get()) {
    msg.str("");
    msg << "mpv: Resuming playback, audio time now "
        << time_base + g_timing_info.load().last_fb_seek + vis_time -
               g_timing_info.load().last_seek_vistime;
    console::info(msg.str().c_str());
  }
}

bool mpv_player::load_mpv() {
  HMODULE dll;
  pfc::string_formatter path = core_api::get_my_full_path();
  path.truncate(path.scan_filename());
  std::wstringstream wpath_mpv;
  wpath_mpv << path << "mpv\\mpv-1.dll";
  dll = LoadLibraryExW(wpath_mpv.str().c_str(), NULL,
                       LOAD_WITH_ALTERED_SEARCH_PATH);
  if (dll == NULL) {
    std::stringstream error;
    error << "Could not load mpv-1.dll: error code " << GetLastError();
    console::error(error.str().c_str());
    return false;
  }

  _mpv_error_string = (mpv_error_string)GetProcAddress(dll, "mpv_error_string");
  _mpv_free = (mpv_free)GetProcAddress(dll, "mpv_free");
  _mpv_client_name = (mpv_client_name)GetProcAddress(dll, "mpv_client_name");
  _mpv_client_id = (mpv_client_id)GetProcAddress(dll, "mpv_client_id");
  _mpv_create = (mpv_create)GetProcAddress(dll, "mpv_create");
  _mpv_initialize = (mpv_initialize)GetProcAddress(dll, "mpv_initialize");
  _mpv_destroy = (mpv_destroy)GetProcAddress(dll, "mpv_destroy");
  _mpv_terminate_destroy =
      (mpv_terminate_destroy)GetProcAddress(dll, "mpv_terminate_destroy");
  _mpv_create_client =
      (mpv_create_client)GetProcAddress(dll, "mpv_create_client");
  _mpv_create_weak_client =
      (mpv_create_weak_client)GetProcAddress(dll, "mpv_create_weak_client");
  _mpv_load_config_file =
      (mpv_load_config_file)GetProcAddress(dll, "mpv_load_config_file");
  _mpv_get_time_us = (mpv_get_time_us)GetProcAddress(dll, "mpv_get_time_us");
  _mpv_free_node_contents =
      (mpv_free_node_contents)GetProcAddress(dll, "mpv_free_node_contents");
  _mpv_set_option = (mpv_set_option)GetProcAddress(dll, "mpv_set_option");
  _mpv_set_option_string =
      (mpv_set_option_string)GetProcAddress(dll, "mpv_set_option_string");
  _mpv_command = (mpv_command)GetProcAddress(dll, "mpv_command");
  _mpv_command_node = (mpv_command_node)GetProcAddress(dll, "mpv_command_node");
  _mpv_command_ret = (mpv_command_ret)GetProcAddress(dll, "mpv_command_ret");
  _mpv_command_string =
      (mpv_command_string)GetProcAddress(dll, "mpv_command_string");
  _mpv_command_async =
      (mpv_command_async)GetProcAddress(dll, "mpv_command_async");
  _mpv_command_node_async =
      (mpv_command_node_async)GetProcAddress(dll, "mpv_command_node_async");
  _mpv_abort_async_command =
      (mpv_abort_async_command)GetProcAddress(dll, "mpv_abort_async_command");
  _mpv_set_property = (mpv_set_property)GetProcAddress(dll, "mpv_set_property");
  _mpv_set_property_string =
      (mpv_set_property_string)GetProcAddress(dll, "mpv_set_property_string");
  _mpv_set_property_async =
      (mpv_set_property_async)GetProcAddress(dll, "mpv_set_property_async");
  _mpv_get_property = (mpv_get_property)GetProcAddress(dll, "mpv_get_property");
  _mpv_get_property_string =
      (mpv_get_property_string)GetProcAddress(dll, "mpv_get_property_string");
  _mpv_get_property_osd_string = (mpv_get_property_osd_string)GetProcAddress(
      dll, "mpv_get_property_osd_string");
  _mpv_get_property_async =
      (mpv_get_property_async)GetProcAddress(dll, "mpv_get_property_async");
  _mpv_observe_property =
      (mpv_observe_property)GetProcAddress(dll, "mpv_observe_property");
  _mpv_unobserve_property =
      (mpv_unobserve_property)GetProcAddress(dll, "mpv_unobserve_property");
  _mpv_event_name = (mpv_event_name)GetProcAddress(dll, "mpv_event_name");
  _mpv_event_to_node =
      (mpv_event_to_node)GetProcAddress(dll, "mpv_event_to_node");
  _mpv_request_event =
      (mpv_request_event)GetProcAddress(dll, "mpv_request_event");
  _mpv_request_log_messages =
      (mpv_request_log_messages)GetProcAddress(dll, "mpv_request_log_messages");
  _mpv_wait_event = (mpv_wait_event)GetProcAddress(dll, "mpv_wait_event");
  _mpv_wakeup = (mpv_wakeup)GetProcAddress(dll, "mpv_wakeup");
  _mpv_set_wakeup_callback =
      (mpv_set_wakeup_callback)GetProcAddress(dll, "mpv_set_wakeup_callback");
  _mpv_wait_async_requests =
      (mpv_wait_async_requests)GetProcAddress(dll, "mpv_wait_async_requests");
  _mpv_hook_add = (mpv_hook_add)GetProcAddress(dll, "mpv_hook_add");
  _mpv_hook_continue =
      (mpv_hook_continue)GetProcAddress(dll, "mpv_hook_continue");

  return true;
}

CMpvWindow::CMpvWindow() {
  container = mpv_container::get_main();
  if (!container) {
    throw exception_messagebox(
        "mpv: Exception creating player window - nowhere to place mpv");
  } else {
    HWND wid;
    WIN32_OP(wid = Create(container->container_wnd(), 0, 0, WS_CHILD, 0));
    mpv_set_wid(wid);
    mpv_update_visibility();
  }
}

mpv_container* CMpvWindow::get_container() { return container; }

void CMpvWindow::update() {
  container = mpv_container::get_main();
  if (!fullscreen_) {
    if (GetParent() != container->container_wnd()) {
      SetParent(container->container_wnd());
    }
    ResizeClient(container->x, container->y);
  }

  mpv_update_visibility();
}

bool CMpvWindow::mpv_is_visible() {
  return fullscreen_ || container->is_visible();
}

BOOL CMpvWindow::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(0x00000000) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}

void CMpvWindow::on_destroy() { mpv_terminate(); }

void CMpvWindow::on_keydown(UINT key, WPARAM, LPARAM) {
  switch (key) {
    case VK_ESCAPE:
      if (fullscreen_) toggle_fullscreen();
    default:
      break;
  }
}

void CMpvWindow::toggle_fullscreen() {
  if (!fullscreen_) {
    saved_style = GetWindowLong(GWL_STYLE);
    saved_ex_style = GetWindowLong(GWL_EXSTYLE);
  }
  fullscreen_ = !fullscreen_;
  if (fullscreen_) {
    SetParent(NULL);
    SetWindowLong(GWL_STYLE,
                  saved_style & ~(WS_CHILD | WS_CAPTION | WS_THICKFRAME) |
                      WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU);
    SetWindowLong(GWL_EXSTYLE, saved_ex_style);

    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(MonitorFromWindow(get_wnd(), MONITOR_DEFAULTTONEAREST),
                    &monitor_info);
    SetWindowPos(NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
  } else {
    SetParent(container->container_wnd());
    SetWindowLong(GWL_STYLE, saved_style);
    SetWindowLong(GWL_EXSTYLE, saved_ex_style);
    SetWindowPos(NULL, 0, 0, container->x, container->y,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ::SetActiveWindow(container->container_wnd());
    ::SetFocus(container->container_wnd());
  }
  container->on_fullscreen(fullscreen_);
}

void CMpvWindow::on_double_click(UINT, CPoint) { toggle_fullscreen(); }

LRESULT CMpvWindow::on_create(LPCREATESTRUCT lpcreate) {
  ShowWindow(SW_SHOW);
  return 0;
}

void CMpvWindow::on_context_menu(CWindow wnd, CPoint point) {
  try {
    {
      // handle the context menu key case - center the menu
      if (point == CPoint(-1, -1)) {
        CRect rc;
        WIN32_OP(wnd.GetWindowRect(&rc));
        point = rc.CenterPoint();
      }

      CMenuDescriptionHybrid menudesc(
          *this);  // this class manages all the voodoo necessary for
                   // descriptions of our menu items to show in the status bar.

      static_api_ptr_t<contextmenu_manager> api;
      CMenu menu;
      WIN32_OP(menu.CreatePopupMenu());
      enum {
        ID_ENABLED = 1,
        ID_FULLSCREEN = 2,
      };
      menu.AppendMenu(disabled ? MF_UNCHECKED : MF_CHECKED, ID_ENABLED,
                      _T("Enabled"));
      menudesc.Set(ID_ENABLED, "Enable/disable video playback");
      menu.AppendMenu(fullscreen_ ? MF_CHECKED : MF_UNCHECKED, ID_FULLSCREEN,
                      _T("Fullscreen"));
      menudesc.Set(ID_FULLSCREEN, "Toggle video fullscreen");

      if (!fullscreen_) {
        container->add_menu_items(&menu, &menudesc);
      }

      int cmd =
          menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                              point.x, point.y, menudesc, 0);

      if (cmd > 0) {
        switch (cmd) {
          case ID_ENABLED:
            disabled = !disabled;
            mpv_update_visibility();
            break;
          case ID_FULLSCREEN:
            toggle_fullscreen();
            break;
          default:
            container->handle_menu_cmd(cmd);
            break;
        }
      }
    }
  } catch (std::exception const& e) {
    console::complain("Context menu failure", e);  // rare
  }
}

HWND CMpvWindow::get_wnd() { return m_hWnd; }

}  // namespace mpv