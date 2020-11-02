#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <thread>

#include "../helpers/VolumeMap.h"
#include "../helpers/atl-misc.h"
#include "../helpers/win32_misc.h"
#include "artwork_protocol.h"
#include "fullscreen_window.h"
#include "menu_utils.h"
#include "player.h"
#include "player_window.h"
#include "popup_window.h"
#include "preferences.h"
#include "resource.h"
#include "timing_info.h"

namespace mpv {
extern cfg_bool cfg_video_enabled, cfg_black_fullscreen, cfg_stop_hidden,
    cfg_artwork, cfg_osc, cfg_osc_scalewithvideo, cfg_hwdec, cfg_latency,
    cfg_deint, cfg_gpuhq, cfg_autopopup;
extern cfg_uint cfg_bg_color, cfg_artwork_type, cfg_osc_layout,
    cfg_osc_seekbarstyle, cfg_osc_transparency, cfg_osc_fadeduration,
    cfg_osc_deadzone, cfg_osc_scalewindowed, cfg_osc_scalefullscreen,
    cfg_osc_timeout;
extern advconfig_checkbox_factory cfg_logging, cfg_mpv_logfile, cfg_foo_youtube,
    cfg_ytdl_any, cfg_remote_always_play;
extern advconfig_integer_factory cfg_max_drift, cfg_hard_sync_threshold,
    cfg_hard_sync_interval, cfg_seek_seconds, cfg_remote_offset;

static CWindowAutoLifetime<player_window>* g_player_window;

void player_window::on_containers_change() {
  auto main = player_container::get_main_container();
  if (main && !g_player_window) {
    g_player_window =
        new CWindowAutoLifetime<player_window>(main->container_wnd(), main);
  }

  if (g_player_window) g_player_window->update();
}

void player_window::restart() {
  if (g_player_window) {
    auto main = player_container::get_main_container();
    g_player_window->DestroyWindow();
    g_player_window =
        new CWindowAutoLifetime<player_window>(main->container_wnd(), main);
  }
}

void player_window::toggle_fullscreen() {
  if (g_player_window) {
    g_player_window->container->toggle_fullscreen();
  } else {
    MONITORINFO monitor;
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(core_api::get_main_window(),
                                      MONITOR_DEFAULTTONEAREST),
                    &monitor);

    fullscreen_window::open(false, monitor);
  }
}

struct monitor_result {
  HMONITOR hmon;
  unsigned count;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
                                     LPRECT lprcMonitor, LPARAM dwData) {
  monitor_result* res = reinterpret_cast<monitor_result*>(dwData);
  if ((*res).count == 0 && (*res).hmon == NULL) {
    (*res).hmon = hMonitor;
  }
  (*res).count = (*res).count - 1;
  return true;
}

void player_window::fullscreen_on_monitor(int monitor) {
  monitor_result r;
  r.hmon = NULL;
  r.count = monitor;
  EnumDisplayMonitors(NULL, NULL, MonitorEnumProc,
                      reinterpret_cast<LPARAM>(&r));
  if (r.hmon != NULL) {
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(r.hmon, &monitor_info);
    fullscreen_window::open(false, monitor_info);
  }
}

player_window::player_window(player_container* p_container) {
  metadb_handle_list selection;
  ui_selection_manager::get()->get_selection(selection);

  container = p_container;
  video_enabled =
      cfg_video_enabled && (p_container->is_fullscreen() ||
                            p_container->is_visible() || !cfg_stop_hidden);
}

player_window::~player_window() { g_player_window = NULL; }

BOOL player_window::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(cfg_bg_color) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}

void player_window::add_menu_items(uie::menu_hook_impl& menu_hook) {
  if (g_player_window) {
    menu_hook.add_node(new menu_utils::menu_node_run(
        "Enable video", "Enable/disable video playback", cfg_video_enabled,
        []() {
          cfg_video_enabled = !cfg_video_enabled;
          g_player->update();
        }));

    menu_hook.add_node(new menu_utils::menu_node_run(
        "Fullscreen", "Toggle fullscreen video",
        g_player->container->is_fullscreen(),
        []() { g_player->container->toggle_fullscreen(); }));
  }
}

void player_window::on_context_menu(CWindow wnd, CPoint point) {
  const char* old_value = "1000";
  if (mpv_handle) {
    old_value =
        libmpv::get()->get_property_string(mpv_handle, "cursor-autohide");
  }
  set_property_string("cursor-autohide", "no");
  container->on_context_menu(wnd, point);
  set_property_string("cursor-autohide", old_value);
}

void player_window::on_destroy() { command_string("quit"); }

LRESULT player_window::on_create(LPCREATESTRUCT lpcreate) {
  container->on_gain_player();

  if (video_enabled && playback_control::get()->is_playing()) {
    metadb_handle_ptr handle;
    playback_control::get()->get_now_playing(handle);
    if (handle.is_valid()) {
      timing_info::refresh(false);
      task t;
      t.type = task_type::Play;
      t.play_file = handle;
      t.time = playback_control::get()->playback_get_position();
      queue_task(t);
    }
  } else {
    mpv_state = mpv_state_t::Idle;
    request_artwork();
  }

  return 0;
}

void player_window::on_mouse_move(UINT, CPoint point) { find_window(); }

void player_window::send_message(UINT msg, UINT wp, UINT lp) {
  if (g_player && g_player->find_window()) {
    SendMessage(g_player->mpv_window_hwnd, msg, wp, lp);
  }
}

void player_window::update() {
  player_container* new_container = player_container::get_main_container();
  player_container* old_container = container;
  {
    std::lock_guard<std::mutex> lock_init(init_mutex);
    container = new_container;
  }

  if (container == NULL) {
    DestroyWindow();

    return;
  } else {
    // wine is less buggy if we resize before changing parent
    ResizeClient(container->cx, container->cy);

    if (GetParent() != container->container_wnd()) {
      SetParent(container->container_wnd());
      player_container::invalidate_all_containers();
    }
  }

  if (old_container != new_container && old_container) {
    old_container->on_lose_player();
  }

  if (old_container != new_container && new_container) {
    new_container->on_gain_player();
  }

  const char* osc_cmd_1[] = {
      "script-message", "foobar", "osc-enabled-changed",
      cfg_osc && container->is_osc_enabled() ? "yes" : "no", NULL};
  command(osc_cmd_1);

  set_property_string("fullscreen", container->is_fullscreen() ? "yes" : "no");

  bool vis = container->is_visible();
  if (cfg_video_enabled && (container->is_fullscreen() ||
                            container->is_visible() || !cfg_stop_hidden)) {
    bool starting = !video_enabled;
    video_enabled = true;

    if (starting && playback_control::get()->is_playing()) {
      metadb_handle_ptr handle;
      playback_control::get()->get_now_playing(handle);
      if (handle.is_valid()) {
        timing_info::refresh(false);
        task t;
        t.type = task_type::Play;
        t.play_file = handle;
        t.time = playback_control::get()->playback_get_position();
        queue_task(t);
      }
    }
  } else {
    bool stopping = video_enabled;
    video_enabled = false;

    if (stopping && mpv_state != mpv_state_t::Artwork) {
      task t;
      t.type = task_type::Stop;
      queue_task(t);
    }
  }

  set_background();
  update_title();
}

bool player_window::contained_in(player_container* p_container) {
  return container == p_container;
}

void player_window::update_title() {
  pfc::string8 title;
  format_player_title(title, current_display_item);

  m_player.publish_title(title);
  uSetWindowText(m_hWnd, title);
  container->set_title(title);
}

void player_window::set_background() {
  std::stringstream colorstrings;
  colorstrings << "#";
  t_uint32 bgcolor =
      container->is_fullscreen() && cfg_black_fullscreen ? 0 : cfg_bg_color;
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetRValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetGValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetBValue(bgcolor);
  std::string colorstring = colorstrings.str();
  set_option_string("background", colorstring.c_str());
}

void player_window::on_changed_sorted(metadb_handle_list_cref changed, bool) {
  if (metadb_handle_list_helper::bsearch_by_pointer(
          changed, current_display_item) < UINT_MAX) {
    publish_titleformatting_subscriptions();

    if (mpv_state == mpv_state_t::Artwork) {
      reload_artwork();
    }
  }
}

void player_window::on_selection_changed(metadb_handle_list_cref p_selection) {
  if (mpv_state == mpv_state_t::Idle || mpv_state == mpv_state_t::Artwork ||
      mpv_state == mpv_state_t::Unloaded) {
    request_artwork(p_selection);
  }
}

void player_window::on_volume_change(float new_vol) {
  std::string vol = std::to_string(VolumeMap::DBToSlider(new_vol));
  const char* cmd[] = {"script-message", "foobar", "volume-changed",
                       vol.c_str(), NULL};
  if (g_player) {
    g_player->command(cmd);
  }
}

void player_window::on_playback_starting(
    play_control::t_track_command p_command, bool p_paused) {
  if (g_player) {
    g_player->set_state(mpv_state_t::Preload);
    task t1;
    t1.type = task_type::Pause;
    t1.flag = p_paused;
    g_player->queue_task(t1);
  }
}
void player_window::on_playback_new_track(metadb_handle_ptr p_track) {
  if (g_player) {
    timing_info::refresh(false);

    {
      std::lock_guard<std::mutex> lock(g_player->sync_lock);
      g_player->last_sync_time = 0;
    }

    task t;
    t.type = task_type::Play;
    t.play_file = p_track;
    t.time = 0.0;
    g_player->queue_task(t);
  }
}
void player_window::on_playback_stop(play_control::t_stop_reason p_reason) {
  if (g_player) {
    g_player->update_title();

    if (g_player->mpv_state != mpv_state_t::Artwork) {
      task t;
      t.type = task_type::Stop;
      g_player->queue_task(t);
    }
  }
}
void player_window::on_playback_seek(double p_time) {
  if (g_player) {
    g_player->update_title();

    timing_info::refresh(true);

    {
      std::lock_guard<std::mutex> lock(g_player->sync_lock);
      g_player->last_sync_time = (int)floor(p_time);
    }

    task t;
    t.type = task_type::Seek;
    t.time = p_time;
    t.flag = false;
    g_player->queue_task(t);
  }
}
void player_window::on_playback_pause(bool p_state) {
  if (g_player) {
    g_player->update_title();
    task t;
    t.type = task_type::Pause;
    t.flag = p_state;
    g_player->queue_task(t);
  }
}
void player_window::on_playback_time(double p_time) {
  if (g_player) {
    g_player->update_title();
    g_player->update();
    g_player->sync(p_time);
  }
}

void player_window::on_new_artwork() {
  if (g_player && (g_player->mpv_state == mpv_state_t::Artwork ||
                   g_player->mpv_state == mpv_state_t::Idle ||
                   g_player->mpv_state == mpv_state_t::Unloaded)) {
    task t;
    t.type = task_type::LoadArtwork;
    g_player->queue_task(t);
  } else if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Ignoring loaded artwork";
  }
}

}  // namespace mpv
