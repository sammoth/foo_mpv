#include "stdafx.h"
// PCH ^
#include "libmpv.h"

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
    "Mpv", guid_cfg_mpv_branch, advconfig_branch::guid_branch_playback, 0);
static advconfig_integer_factory cfg_mpv_max_drift(
    "Permitted timing drift (ms)", guid_cfg_mpv_max_drift, guid_cfg_mpv_branch,
    0, 20, 0, 1000, 0);
static advconfig_integer_factory cfg_mpv_hard_sync("Hard sync threshold (ms)",
                                                   guid_cfg_mpv_hard_sync,
                                                   guid_cfg_mpv_branch, 0, 2000,
                                                   0, 10000, 0);
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

mpv_player::mpv_player() : wid(NULL), enabled(false), mpv(NULL) {
  if (!load_mpv()) {
    console::error("Could not load mpv-1.dll");
  }
}

void mpv_player::init() {
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
      _mpv_terminate_destroy(mpv);
      mpv = NULL;
    }
  }
}

void mpv_player::terminate() {
  enabled = false;

  if (mpv != NULL) {
    mpv_handle* temp = mpv;
    mpv = NULL;
    _mpv_terminate_destroy(temp);
  }
};

void mpv_player::enable() {
  bool starting = !enabled;
  enabled = true;

  if (starting && playback_control::get()->is_playing()) {
    metadb_handle_ptr handle;
    playback_control::get()->get_now_playing(handle);
    play_path(handle->get_path());
  }
}

void mpv_player::disable() {
  if (cfg_mpv_stop_hidden) {
    stop();
    enabled = false;
  }
}

void mpv_player::set_mpv_wid(HWND wnd) { wid = wnd; }

void mpv_player::play_path(const char* metadb_path) {
  if (!enabled) return;

  if (mpv == NULL) init();

  std::stringstream time_sstring;
  time_sstring.setf(std::ios::fixed);
  time_sstring.precision(3);
  time_sstring << playback_control::get()->playback_get_position();
  std::string time_string = time_sstring.str();
  _mpv_set_option_string(mpv, "start", time_string.c_str());

  pfc::string8 filename;
  filename.add_filename(metadb_path);
  if (filename.has_prefix("\\file://")) {
    filename.remove_chars(0, 8);

    if (filename.is_empty()) return;

    const char* cmd[] = {"loadfile", filename.c_str(), NULL};
    if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get()) {
      std::stringstream msg;
      msg << "Mpv: Error loading item '" << filename << "'";
      console::error(msg.str().c_str());
    }
  } else if (cfg_mpv_logging.get()) {
    std::stringstream msg;
    msg << "Mpv: Skipping loading item '" << filename
        << "' because it is not a local file";
    console::error(msg.str().c_str());
  }
}

void mpv_player::stop() {
  if (mpv == NULL || !enabled) return;

  if (_mpv_command_string(mpv, "stop") < 0 && cfg_mpv_logging.get()) {
    console::error("Mpv: Error stopping video");
  }
}

void mpv_player::pause(bool state) {
  if (mpv == NULL || !enabled) return;

  if (_mpv_set_property_string(mpv, "pause", state ? "yes" : "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("Mpv: Error pausing");
  }
}

void mpv_player::seek(double time) {
  if (mpv == NULL || !enabled) return;

  std::stringstream time_sstring;
  time_sstring.setf(std::ios::fixed);
  time_sstring.precision(15);
  time_sstring << time;
  std::string time_string = time_sstring.str();
  const char* cmd[] = {"seek", time_string.c_str(), "absolute+exact", NULL};
  if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get()) {
    console::error("Mpv: Error seeking");
  }
}

void mpv_player::sync() {
  if (mpv == NULL || !enabled) return;

  double mpv_time = -1.0;
  if (_mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
    return;

  double desync = playback_control::get()->playback_get_position() - mpv_time;
  double new_speed = 1.0;

  if (abs(desync) > 0.001 * cfg_mpv_hard_sync.get()) {
    // hard sync
    seek(playback_control::get()->playback_get_position());
    if (cfg_mpv_logging.get()) {
      console::info("Mpv: A/V sync");
    }
  } else {
    // soft sync
    if (abs(desync) > 0.001 * cfg_mpv_max_drift.get()) {
      // aim to correct mpv internal timer in 1 second, then let mpv catch up
      // the video
      new_speed = min(max(1.0 + desync, 0.01), 100.0);
    }
  }

  if (cfg_mpv_logging.get()) {
    std::stringstream msg;
    msg.setf(std::ios::fixed);
    msg.setf(std::ios::showpos);
    msg.precision(10);
    msg << "Mpv: Video offset " << desync << "; setting mpv speed to "
        << new_speed;

    console::info(msg.str().c_str());
  }

  if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
      cfg_mpv_logging.get()) {
    console::error("Mpv: Error setting speed");
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