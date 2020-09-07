#include "stdafx.h"
// PCH ^
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

mpv_player::mpv_player()
    : wid(NULL),
      enabled(false),
      mpv(NULL),
      time_base(0),
      current_sync_request(0) {
  mpv_loaded = load_mpv();
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

    // foobar plays the audio
    _mpv_set_option_string(mpv, "audio", "no");

    // start timing immediately to keep in sync
    _mpv_set_option_string(mpv, "no-initial-audio-sync", "yes");

    // keep the renderer initialised
    _mpv_set_option_string(mpv, "force-window", "yes");
    _mpv_set_option_string(mpv, "idle", "yes");

    if (_mpv_initialize(mpv) != 0) {
      _mpv_destroy(mpv);
      mpv = NULL;
    }
  }
}

void mpv_player::mpv_terminate() {
  if (!mpv_loaded) return;

  enabled = false;

  if (mpv != NULL) {
    mpv_handle* temp = mpv;
    mpv = NULL;
    _mpv_terminate_destroy(temp);
  }
};

void mpv_player::mpv_enable() {
  if (!mpv_loaded) return;

  bool starting = !enabled;
  enabled = true;

  if (starting && playback_control::get()->is_playing()) {
    metadb_handle_ptr handle;
    playback_control::get()->get_now_playing(handle);
    mpv_play(handle);
  }
}

void mpv_player::mpv_disable() {
  if (!mpv_loaded) return;

  if (cfg_mpv_stop_hidden) {
    mpv_stop();
    enabled = false;
  }
}

void mpv_player::mpv_set_wid(HWND wnd) { wid = wnd; }

void mpv_player::on_playback_starting(play_control::t_track_command p_command,
                                      bool p_paused) {}
void mpv_player::on_playback_new_track(metadb_handle_ptr p_track) {
  mpv_play(p_track);
}
void mpv_player::on_playback_stop(play_control::t_stop_reason p_reason) {
  mpv_stop();
}
void mpv_player::on_playback_seek(double p_time) {
  mpv_seek(p_time);
  if (playback_control::get()->is_paused()) {
    current_sync_request++;
  } else {
  mpv_request_initial_sync();
  }
}
void mpv_player::on_playback_pause(bool p_state) { mpv_pause(p_state); }
void mpv_player::on_playback_time(double p_time) { mpv_sync(); }

void mpv_player::mpv_play(metadb_handle_ptr metadb) {
  if (!mpv_loaded) return;

  if (!enabled) return;

  if (mpv == NULL) mpv_init();

  pfc::string8 filename;
  filename.add_filename(metadb->get_path());
  if (filename.has_prefix("\\file://")) {
    filename.remove_chars(0, 8);

    if (filename.is_empty()) return;

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

    std::stringstream time_sstring;
    time_sstring.setf(std::ios::fixed);
    time_sstring.precision(3);
    double start_time = playback_control::get()->playback_get_position();
    time_sstring << time_base + start_time;
    std::string time_string = time_sstring.str();
    _mpv_set_option_string(mpv, "start", time_string.c_str());

    const char* cmd[] = {"loadfile", filename.c_str(), NULL};
    if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get()) {
      std::stringstream msg;
      msg << "mpv: Error loading item '" << filename << "'";
      console::error(msg.str().c_str());
    }

    last_seek = start_time;
  } else if (cfg_mpv_logging.get()) {
    std::stringstream msg;
    msg << "mpv: Skipping loading item '" << filename
        << "' because it is not a local file";
    console::error(msg.str().c_str());
  }

  mpv_request_initial_sync();
}

void mpv_player::mpv_cancel_sync_requests() { current_sync_request++; }

void mpv_player::mpv_request_initial_sync() {
  int request_number = current_sync_request.fetch_add(1) + 1;
  auto f = [this, request_number]() {
    if (!mpv_loaded) return;

    if (mpv == NULL || !enabled) return;

    const int64_t userdata = 853727396;
    if (_mpv_observe_property(mpv, userdata, "time-pos", MPV_FORMAT_DOUBLE) <
        0) {
      if (cfg_mpv_logging.get()) {
        console::error("mpv: Error observing time-pos");
      }
      return;
    }
    if (_mpv_observe_property(mpv, userdata, "pause", MPV_FORMAT_DOUBLE) < 0) {
      if (cfg_mpv_logging.get()) {
        console::error("mpv: Error observing time-pos");
      }
      return;
    }

    double mpv_time = -1.0;
    while (true) {
      if (current_sync_request != request_number) {
        _mpv_unobserve_property(mpv, userdata);
        return;
      }
      mpv_event* event = _mpv_wait_event(mpv, 0.01);
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
          return;  // paused while waiting
        }
        if (event_property->format != MPV_FORMAT_DOUBLE)
          continue;  // no frame decoded yet
        mpv_time = *(double*)(event_property->data);

        if (mpv_time > last_seek) {
          // frame decoded, wait for fb
          if (current_sync_request != request_number) {
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

    visualisation_stream::ptr vis_stream;
    visualisation_manager::get()->create_stream(vis_stream, 0);

    // wait for fb to catch up to the first frame
    int timeout = 0;
    double fb_time = 0.0;
    vis_stream->get_absolute_time(fb_time);
    while (timeout < 100 && time_base + last_seek + fb_time < mpv_time) {
      timeout++;
      Sleep(10);
      vis_stream->get_absolute_time(fb_time);
    }

    if (current_sync_request != request_number) {
      return;
    }
    if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
        cfg_mpv_logging.get()) {
      console::error("mpv: Error pausing");
    }
  };
  auto t = std::thread(f);
  t.detach();
}

void mpv_player::mpv_stop() {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  if (_mpv_command_string(mpv, "stop") < 0 && cfg_mpv_logging.get()) {
    console::error("mpv: Error stopping video");
  }

  // when foobar is stopped it's also unpaused
  if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("mpv: Error pausing");
  }
}

void mpv_player::mpv_pause(bool state) {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  if (_mpv_set_property_string(mpv, "pause", state ? "yes" : "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("mpv: Error pausing");
  }
}

void mpv_player::mpv_seek(double time) {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  std::stringstream time_sstring;
  time_sstring.setf(std::ios::fixed);
  time_sstring.precision(15);
  time_sstring << time_base + time;
  std::string time_string = time_sstring.str();
  const char* cmd[] = {"seek", time_string.c_str(), "absolute+exact", NULL};
  if (_mpv_command(mpv, cmd) < 0) {
    Sleep(10);  // one more time
    if (_mpv_command(mpv, cmd) < 0) {
      if (cfg_mpv_logging.get()) {
        console::error("mpv: Error seeking");
      }
    }
  }

  last_seek = time;
}

void mpv_player::mpv_sync_initial(double last_seek, unsigned request_number) {
}  // namespace mpv

void mpv_player::mpv_sync() {
  if (!mpv_loaded) return;

  if (mpv == NULL || !enabled) return;

  double mpv_time = -1.0;
  if (_mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
    return;

  double fb_time = playback_control::get()->playback_get_position();
  double desync = time_base + fb_time - mpv_time;
  double new_speed = 1.0;

  if (abs(desync) > 0.001 * cfg_mpv_hard_sync.get() &&
      (fb_time - last_seek) > cfg_mpv_hard_sync_interval.get()) {
    // hard sync
    mpv_seek(fb_time);
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
      msg << "mpv: Video offset " << desync << "; setting mpv speed to "
          << new_speed;

      console::info(msg.str().c_str());
    }

    if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_mpv_logging.get()) {
      console::error("mpv: Error setting speed");
    }
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

CMpvWindow::CMpvWindow(HWND parent) : parent_(parent) {
  HWND wid;
  WIN32_OP(wid = Create(parent, 0, 0, WS_CHILD, 0));
  mpv_set_wid(wid);
}

BOOL CMpvWindow::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(0x00000000) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}

void CMpvWindow::MaybeResize(LONG x, LONG y) {
  x_ = x;
  y_ = y;

  if (!fullscreen_) ResizeClient(x_, y_);
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
    SetParent(parent_);
    SetWindowLong(GWL_STYLE, saved_style);
    SetWindowLong(GWL_EXSTYLE, saved_ex_style);
    SetWindowPos(NULL, 0, 0, x_, y_,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ::SetActiveWindow(parent_);
    ::SetFocus(parent_);
  }
}

void CMpvWindow::on_double_click(UINT, CPoint) { toggle_fullscreen(); }

LRESULT CMpvWindow::on_create(LPCREATESTRUCT lpcreate) {
  ShowWindow(SW_SHOW);
  return 0;
}

HWND CMpvWindow::get_wnd() { return m_hWnd; }
}  // namespace mpv