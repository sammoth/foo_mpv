#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <set>
#include <thread>

#include "../helpers/atl-misc.h"
#include "../helpers/win32_misc.h"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"
#include "timing_info.h"

namespace mpv {

extern cfg_bool cfg_video_enabled, cfg_black_fullscreen, cfg_stop_hidden;
extern cfg_uint cfg_bg_color;
extern advconfig_checkbox_factory cfg_logging, cfg_mpv_logfile;
extern advconfig_integer_factory cfg_max_drift, cfg_hard_sync_threshold,
    cfg_hard_sync_interval;

mpv_player::mpv_player()
    : enabled(false),
      mpv(NULL),
      sync_task(sync_task_type::Wait),
      sync_on_unpause(false),
      last_mpv_seek(0),
      last_hard_sync(-99),
      time_base(0) {
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
            sync_thread_sync();
            sync_task = sync_task_type::Stop;
            break;
          default:
            break;
        }
      }
    }
  });

  update_container();
}

mpv_player::~mpv_player() {
  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Quit;
  }
  cv.notify_all();
  sync_thread.join();
}

void mpv_player::destroy() { DestroyWindow(); }

BOOL mpv_player::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(cfg_bg_color) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}
void mpv_player::on_keydown(UINT key, WPARAM, LPARAM) {
  switch (key) {
    case VK_ESCAPE:
      if (fullscreen_) toggle_fullscreen();
    default:
      break;
  }
}

enum { ID_ENABLED = 1, ID_FULLSCREEN = 2, ID_STATS = 99 };
void mpv_player::add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
  if (mpv != NULL) {
    if (is_idle()) {
      menu->AppendMenu(MF_DISABLED, ID_STATS, _T("Idle"));
    } else {
      std::wstringstream text;
      text.setf(std::ios::fixed);
      text.precision(3);
      text << get_string("video-codec") << " "
           << get_string("video-params/pixelformat");
      menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      text.str(L"");
      text << get_string("width") << "x" << get_string("height") << " "
           << get_double("container-fps") << "fps (display "
           << get_double("estimated-vf-fps") << "fps)";
      menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      std::string hwdec = get_string("hwdec-current");
      if (hwdec != "no") {
        text.str(L"");
        text << "Hardware decoding: " << get_string("hwdec-current");
        menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      }

      menu->AppendMenu(MF_SEPARATOR, ID_STATS, _T(""));
    }
  }

  menu->AppendMenu(cfg_video_enabled ? MF_CHECKED : MF_UNCHECKED, ID_ENABLED,
                   _T("Enabled"));
  menudesc->Set(ID_ENABLED, "Enable/disable video playback");
  menu->AppendMenu(fullscreen_ ? MF_CHECKED : MF_UNCHECKED, ID_FULLSCREEN,
                   _T("Fullscreen"));
  menudesc->Set(ID_FULLSCREEN, "Toggle video fullscreen");
}

void mpv_player::handle_menu_cmd(int cmd) {
  switch (cmd) {
    case ID_ENABLED:
      cfg_video_enabled = !cfg_video_enabled;
      update();
      break;
    case ID_FULLSCREEN:
      toggle_fullscreen();
      break;
    default:
      break;
  }
}

void mpv_player::on_context_menu(CWindow wnd, CPoint point) {
  if (fullscreen_) {
    try {
      {
        // handle the context menu key case - center the menu
        if (point == CPoint(-1, -1)) {
          CRect rc;
          WIN32_OP(wnd.GetWindowRect(&rc));
          point = rc.CenterPoint();
        }

        CMenuDescriptionHybrid menudesc(*this);

        static_api_ptr_t<contextmenu_manager> api;
        CMenu menu;
        WIN32_OP(menu.CreatePopupMenu());

        add_menu_items(&menu, &menudesc);

        int cmd =
            menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                                point.x, point.y, menudesc, 0);

        handle_menu_cmd(cmd);
      }
    } catch (std::exception const& e) {
      console::complain("Context menu failure", e);
    }
  } else {
    container->container_on_context_menu(wnd, point);
  }
}

void mpv_player::on_double_click(UINT, CPoint) { toggle_fullscreen(); }

void mpv_player::toggle_fullscreen() {
  if (!fullscreen_) {
    saved_style = GetWindowLong(GWL_STYLE);
    saved_ex_style = GetWindowLong(GWL_EXSTYLE);
  }
  fullscreen_ = !fullscreen_;
  update();
  if (fullscreen_) {
    SetParent(NULL);
    SetWindowLong(GWL_STYLE,
                  saved_style & ~(WS_CHILD | WS_CAPTION | WS_THICKFRAME) |
                      WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU);

    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST),
                    &monitor_info);
    SetWindowPos(NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
  } else {
    SetParent(container->container_wnd());
    SetWindowLong(GWL_STYLE, saved_style);
    SetWindowLong(GWL_EXSTYLE, saved_ex_style);
    SetWindowPos(NULL, 0, 0, container->cx, container->cy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  container->on_fullscreen(fullscreen_);
}

void mpv_player::on_destroy() { mpv_terminate(); }

LRESULT mpv_player::on_create(LPCREATESTRUCT lpcreate) {
  SetClassLong(
      m_hWnd, GCL_HICON,
      (LONG)LoadIcon(core_api::get_my_instance(), MAKEINTRESOURCE(IDI_ICON1)));
  update();
  return 0;
}

void mpv_player::update_container() { container = get_main_container(); }

void mpv_player::update() {
  update_container();
  update_window();
}

void mpv_player::update_window() {
  if (!fullscreen_) {
    if (GetParent() != container->container_wnd()) {
      SetParent(container->container_wnd());
      invalidate_all_containers();
    }
    ResizeClient(container->cx, container->cy);
  }

  bool vis = container->is_visible();
  if (cfg_video_enabled &&
      (fullscreen_ || container->is_visible() || !cfg_stop_hidden)) {
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

  set_background();
}

bool mpv_player::contained_in(mpv_container* p_container) {
  return container == p_container;
}

void mpv_player::update_title() {
  pfc::string8 title;
  mpv::get_popup_title(title);
  uSetWindowText(m_hWnd, title);
}

void mpv_player::set_background() {
  if (mpv == NULL) return;

  std::stringstream colorstrings;
  colorstrings << "#";
  t_uint32 bgcolor = fullscreen_ && cfg_black_fullscreen ? 0 : cfg_bg_color;
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetRValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetGValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetBValue(bgcolor);
  std::string colorstring = colorstrings.str();
  libmpv()->set_option_string(mpv, "background", colorstring.c_str());
}

bool mpv_player::mpv_init() {
  if (!libmpv()->load_dll()) return false;

  if (mpv == NULL && m_hWnd != NULL) {
    pfc::string_formatter path;
    path.add_filename(core_api::get_profile_path());
    path.add_filename("mpv");
    path.replace_string("\\file://", "");
    mpv = libmpv()->create();

    int64_t l_wid = (intptr_t)(m_hWnd);
    libmpv()->set_option(mpv, "wid", MPV_FORMAT_INT64, &l_wid);

    libmpv()->set_option_string(mpv, "load-scripts", "no");
    libmpv()->set_option_string(mpv, "ytdl", "no");
    libmpv()->set_option_string(mpv, "load-stats-overlay", "no");
    libmpv()->set_option_string(mpv, "load-osd-console", "no");

    set_background();

    libmpv()->set_option_string(mpv, "config", "yes");
    libmpv()->set_option_string(mpv, "config-dir", path.c_str());

    if (cfg_mpv_logfile) {
      path.add_filename("mpv.log");
      libmpv()->set_option_string(mpv, "log-file", path.c_str());
    }

    // no display for music
    libmpv()->set_option_string(mpv, "audio-display", "no");

    // everything syncs to foobar
    libmpv()->set_option_string(mpv, "video-sync", "audio");
    libmpv()->set_option_string(mpv, "untimed", "no");

    // seek fast
    libmpv()->set_option_string(mpv, "hr-seek-framedrop", "yes");
    libmpv()->set_option_string(mpv, "hr-seek-demuxer-offset", "0");

    // foobar plays the audio
    libmpv()->set_option_string(mpv, "audio", "no");

    // start timing immediately to keep in sync
    libmpv()->set_option_string(mpv, "no-initial-audio-sync", "yes");

    // keep the renderer initialised
    libmpv()->set_option_string(mpv, "force-window", "yes");
    libmpv()->set_option_string(mpv, "idle", "yes");

    // don't unload the file when finished, maybe fb is still playing and we
    // could be asked to seek backwards
    libmpv()->set_option_string(mpv, "keep-open", "yes");

    if (libmpv()->initialize(mpv) != 0) {
      libmpv()->destroy(mpv);
      mpv = NULL;
    }
  }

  return mpv != NULL;
}

void mpv_player::mpv_terminate() {
  if (mpv != NULL) {
    mpv_handle* temp = mpv;
    mpv = NULL;
    libmpv()->terminate_destroy(temp);
  }
};

void mpv_player::on_playback_starting(play_control::t_track_command p_command,
                                      bool p_paused) {
    mpv_pause(p_paused);
}
void mpv_player::on_playback_new_track(metadb_handle_ptr p_track) {
  update_title();
  update();
  mpv_play(p_track, true);
}
void mpv_player::on_playback_stop(play_control::t_stop_reason p_reason) {
  update_title();
  mpv_stop();
}
void mpv_player::on_playback_seek(double p_time) {
  update_title();
  mpv_seek(p_time);
}
void mpv_player::on_playback_pause(bool p_state) {
  update_title();
  mpv_pause(p_state);
}
void mpv_player::on_playback_time(double p_time) {
  update_title();
  update();
  mpv_sync(p_time);
}

void mpv_player::mpv_play(metadb_handle_ptr metadb, bool new_file) {
  if (!enabled) return;
  if (mpv == NULL && !mpv_init()) return;

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

      double time_base_l = 0.0;
      if (metadb->get_subsong_index() > 1) {
        for (t_uint32 s = 0; s < metadb->get_subsong_index(); s++) {
          playable_location_impl tmp = metadb->get_location();
          tmp.set_subsong(s);
          metadb_handle_ptr subsong = metadb::get()->handle_create(tmp);
          if (subsong.is_valid()) {
            time_base_l += subsong->get_length();
          }
        }
      }
      time_base = time_base_l;

      double start_time =
          new_file ? 0.0 : playback_control::get()->playback_get_position();
      last_mpv_seek = ceil(1000 * start_time) / 1000.0;
      last_hard_sync = -99;

      std::stringstream time_sstring;
      time_sstring.setf(std::ios::fixed);
      time_sstring.precision(3);
      time_sstring << time_base + start_time;
      std::string time_string = time_sstring.str();
      libmpv()->set_option_string(mpv, "start", time_string.c_str());
      if (cfg_logging) {
        std::stringstream msg;
        msg << "mpv: Loading item '" << filename << "' at start time "
            << time_base + start_time;
        console::info(msg.str().c_str());
      }

      // reset speed
      double unity = 1.0;
      if (libmpv()->set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &unity) < 0 &&
          cfg_logging) {
        console::error("mpv: Error setting speed");
      }

      const char* cmd[] = {"loadfile", filename.c_str(), NULL};
      if (libmpv()->command(mpv, cmd) < 0 && cfg_logging) {
        std::stringstream msg;
        msg << "mpv: Error loading item '" << filename << "'";
        console::error(msg.str().c_str());
      }

      if (playback_control::get()->is_paused()) {
        if (libmpv()->set_property_string(mpv, "pause", "yes") < 0 &&
            cfg_logging) {
          console::error("mpv: Error pausing");
        }
        sync_on_unpause = true;
        if (cfg_logging) {
          console::info("mpv: Setting sync_on_unpause after load");
        }
      } else {
        if (libmpv()->set_property_string(mpv, "pause", "no") < 0 &&
            cfg_logging) {
          console::error("mpv: Error pausing");
        }
        sync_task = sync_task_type::FirstFrameSync;
        if (cfg_logging) {
          console::info("mpv: Starting first frame sync after load");
        }
      }
    } else if (cfg_logging) {
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

    if (libmpv()->command_string(mpv, "stop") < 0 && cfg_logging) {
      console::error("mpv: Error stopping video");
    }

    lock.unlock();
  }
  cv.notify_all();
}

bool mpv_player::is_idle() {
  int idle = 0;
  libmpv()->get_property(mpv, "idle-active", MPV_FORMAT_FLAG, &idle);
  return idle == 1;
}

void mpv_player::mpv_pause(bool state) {
  if (mpv == NULL || !enabled || is_idle()) return;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    if (cfg_logging) {
      console::info(state ? "mpv: Pause" : "mpv: Unpause");
    }
    if (libmpv()->set_property_string(mpv, "pause", state ? "yes" : "no") < 0 &&
        cfg_logging) {
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

void mpv_player::mpv_seek(double time) {
  if (mpv == NULL || !enabled || is_idle()) return;

  {
    std::lock_guard<std::mutex> lock(cv_mutex);
    sync_task = sync_task_type::Stop;
  }
  cv.notify_all();

  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [this] { return sync_task == sync_task_type::Wait; });

    if (cfg_logging) {
      std::stringstream msg;
      msg << "mpv: Seeking to " << time;
      console::info(msg.str().c_str());
    }

    last_mpv_seek = time;
    last_hard_sync = -99;
    // reset speed
    double unity = 1.0;
    if (libmpv()->set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &unity) < 0 &&
        cfg_logging) {
      console::error("mpv: Error setting speed");
    }

    // build command
    std::stringstream time_sstring;
    time_sstring.setf(std::ios::fixed);
    time_sstring.precision(15);
    time_sstring << time_base + time;
    std::string time_string = time_sstring.str();
    const char* cmd[] = {"seek", time_string.c_str(), "absolute+exact", NULL};

    if (libmpv()->command(mpv, cmd) < 0) {
      if (cfg_logging) {
        console::error("mpv: Error seeking, waiting for file to load first");
      }

      const int64_t userdata = 853727396;
      if (libmpv()->observe_property(mpv, userdata, "seeking",
                                     MPV_FORMAT_FLAG) < 0) {
        if (cfg_logging) {
          console::error("mpv: Error observing seeking");
        }
      } else {
        for (int i = 0; i < 100; i++) {
          mpv_event* event = libmpv()->wait_event(mpv, 0.05);

          if (is_idle()) {
            libmpv()->unobserve_property(mpv, userdata);
            return;
          }

          if (event->event_id == MPV_EVENT_SHUTDOWN) {
            libmpv()->unobserve_property(mpv, userdata);
            break;
          }

          if (event->event_id == MPV_EVENT_PROPERTY_CHANGE &&
              event->reply_userdata == userdata) {
            mpv_event_property* event_property =
                (mpv_event_property*)event->data;

            if (event_property->format != MPV_FORMAT_FLAG) continue;

            int seeking = *(int*)(event_property->data);
            if (seeking == 0) {
              break;
            }
          }
        }
      }
      libmpv()->unobserve_property(mpv, userdata);

      if (libmpv()->command(mpv, cmd) < 0) {
        if (cfg_logging) {
          console::error("mpv: Error seeking");
        }
      }
    }

    if (playback_control::get()->is_paused()) {
      sync_on_unpause = true;
      if (cfg_logging) {
        console::info("mpv: Queueing sync after paused seek");
      }
    } else {
      sync_task = sync_task_type::FirstFrameSync;
      if (cfg_logging) {
        console::info("mpv: Starting first frame sync after seek");
      }
    }

    lock.unlock();
  }
  cv.notify_all();
}

void mpv_player::mpv_sync(double debug_time) {
  if (mpv == NULL || !enabled || is_idle()) return;

  if (playback_control::get()->is_paused()) {
    return;
  }

  double mpv_time = -1.0;
  if (libmpv()->get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
    return;

  double fb_time = playback_control::get()->playback_get_position();
  double desync = time_base + fb_time - mpv_time;
  double new_speed = 1.0;

  if (abs(desync) > 0.001 * cfg_hard_sync_threshold &&
      (fb_time - last_hard_sync) > cfg_hard_sync_interval) {
    // hard sync
    timing_info::force_refresh();
    mpv_seek(fb_time);
    last_hard_sync = fb_time;
    if (cfg_logging) {
      console::info("mpv: Hard a/v sync");
    }
  } else {
    if (sync_task == sync_task_type::FirstFrameSync) {
      if (cfg_logging) {
        console::info("mpv: Skipping regular sync");
      }
      return;
    }

    // soft sync
    if (abs(desync) > 0.001 * cfg_max_drift) {
      // aim to correct mpv internal timer in 1 second, then let mpv catch up
      // the video
      new_speed = min(max(1.0 + desync, 0.01), 100.0);
    }

    if (cfg_logging) {
      std::stringstream msg;
      msg.setf(std::ios::fixed);
      msg.setf(std::ios::showpos);
      msg.precision(10);
      msg << "mpv: Sync at " << debug_time << " video offset " << desync
          << "; setting mpv speed to " << new_speed;
      console::info(msg.str().c_str());
    }

    if (libmpv()->set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_logging) {
      console::error("mpv: Error setting speed");
    }
  }
}

void mpv_player::sync_thread_sync() {
  if (mpv == NULL || !enabled) return;

  sync_on_unpause = false;

  std::stringstream msg;
  msg.setf(std::ios::fixed);
  msg.setf(std::ios::showpos);
  msg.precision(10);

  int paused_check = 0;
  libmpv()->get_property(mpv, "pause", MPV_FORMAT_FLAG, &paused_check);
  if (paused_check == 1) {
    console::error(
        "mpv: [sync thread] Player was paused when starting initial sync");
    console::info("mpv: [sync thread] Abort");
    return;
  }

  if (cfg_logging) {
    console::info("mpv: [sync thread] Initial sync");
  }

  const int64_t userdata = 208341047;
  if (libmpv()->observe_property(mpv, userdata, "time-pos", MPV_FORMAT_DOUBLE) <
      0) {
    if (cfg_logging) {
      console::error("mpv: [sync thread] Error observing time-pos");
    }
    return;
  }

  double mpv_time = -1.0;
  while (true) {
    if (sync_task != sync_task_type::FirstFrameSync) {
      libmpv()->unobserve_property(mpv, userdata);
      if (cfg_logging) {
        console::info("mpv: [sync thread] Abort");
      }
      return;
    }

    mpv_event* event = libmpv()->wait_event(mpv, 0.01);
    if (is_idle()) {
      libmpv()->unobserve_property(mpv, userdata);
      if (cfg_logging) {
        console::info("mpv: [sync thread] Abort");
      }
      return;
    }

    if (event->event_id == MPV_EVENT_SHUTDOWN) {
      libmpv()->unobserve_property(mpv, userdata);
      if (cfg_logging) {
        console::info("mpv: [sync thread] Abort");
      }
      return;
    }

    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE &&
        event->reply_userdata == userdata) {
      mpv_event_property* event_property = (mpv_event_property*)event->data;

      if (event_property->format != MPV_FORMAT_DOUBLE)
        continue;  // no frame decoded yet

      mpv_time = *(double*)(event_property->data);
      if (mpv_time > last_mpv_seek) {
        // frame decoded, wait for fb
        if (cfg_logging) {
          msg.str("");
          msg << "mpv: [sync thread] First frame found at timestamp "
              << mpv_time << " after seek to " << last_mpv_seek << ", pausing";
          console::info(msg.str().c_str());
        }

        if (sync_task != sync_task_type::FirstFrameSync) {
          libmpv()->unobserve_property(mpv, userdata);
          if (cfg_logging) {
            console::info("mpv: [sync thread] Abort");
          }
          return;
        }

        if (libmpv()->set_property_string(mpv, "pause", "yes") < 0 &&
            cfg_logging) {
          console::error("mpv: [sync thread] Error pausing");
        }

        break;
      }
    }
  }
  libmpv()->unobserve_property(mpv, userdata);

  // wait for fb to catch up to the first frame
  double vis_time = 0.0;
  visualisation_stream::ptr vis_stream = NULL;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  if (!vis_stream.is_valid()) {
    console::error(
        "mpv: [sync thread] Video disabled: this output has no timing "
        "information");
    if (libmpv()->set_property_string(mpv, "pause", "no") < 0 && cfg_logging) {
      console::error("mpv: [sync thread] Error pausing");
    }
    if (cfg_logging) {
      console::info("mpv: [sync thread] Abort");
    }
    fb2k::inMainThread([this]() {
      cfg_video_enabled = false;
      update();
    });
    return;
  }
  vis_stream->get_absolute_time(vis_time);

  if (cfg_logging) {
    msg.str("");
    msg << "mpv: [sync thread] Audio time "
        << time_base + timing_info::get().last_fb_seek + vis_time -
               timing_info::get().last_seek_vistime;
    console::info(msg.str().c_str());
  }

  int count = 0;
  while (time_base + timing_info::get().last_fb_seek + vis_time -
             timing_info::get().last_seek_vistime <
         mpv_time) {
    if (sync_task != sync_task_type::FirstFrameSync) {
      if (libmpv()->set_property_string(mpv, "pause", "no") < 0 &&
          cfg_logging) {
        console::error("mpv: [sync thread] Error pausing");
      }
      if (cfg_logging) {
        console::info("mpv: [sync thread] Abort");
      }
      return;
    }
    Sleep(10);
    vis_stream->get_absolute_time(vis_time);
    if (count++ > 500 && !vis_stream->get_absolute_time(vis_time)) {
      console::error(
          "mpv: [sync thread] Initial sync failed, maybe this output does not "
          "have accurate "
          "timing info");
      if (libmpv()->set_property_string(mpv, "pause", "no") < 0 &&
          cfg_logging) {
        console::error("mpv: [sync thread] Error pausing");
      }
      if (cfg_logging) {
        console::info("mpv: [sync thread] Abort");
      }
      return;
    }
  }

  if (libmpv()->set_property_string(mpv, "pause", "no") < 0 && cfg_logging) {
    console::error("mpv: [sync thread] Error pausing");
  }

  if (cfg_logging) {
    msg.str("");
    msg << "mpv: [sync thread] Resuming playback, audio time now "
        << time_base + timing_info::get().last_fb_seek + vis_time -
               timing_info::get().last_seek_vistime;
    console::info(msg.str().c_str());
  }
}

const char* mpv_player::get_string(const char* name) {
  if (mpv == NULL) return "Error";
  return libmpv()->get_property_string(mpv, name);
}

bool mpv_player::get_bool(const char* name) {
  if (mpv == NULL) return false;
  int flag = 0;
  libmpv()->get_property(mpv, name, MPV_FORMAT_FLAG, &flag);
  return flag == 1;
}

double mpv_player::get_double(const char* name) {
  if (mpv == NULL) return 0;
  double num = 0;
  libmpv()->get_property(mpv, name, MPV_FORMAT_DOUBLE, &num);
  return num;
}
}  // namespace mpv