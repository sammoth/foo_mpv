#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <set>
#include <thread>

#include "helpers/atl-misc.h"
#include "helpers/win32_misc.h"
#include "libmpv.h"
#include "resource.h"

namespace mpv {
static const GUID g_guid_cfg_mpv_bg_color = {
    0xb62c3ef, 0x3c6e, 0x4620, {0xbf, 0xa2, 0x24, 0xa, 0x5e, 0xdd, 0xbc, 0x4b}};
static const GUID g_guid_cfg_mpv_popup_titleformat = {
    0x811a9299,
    0x833d,
    0x4d38,
    {0xb3, 0xee, 0x89, 0x19, 0x8d, 0xed, 0x20, 0x77}};
static const GUID g_guid_cfg_mpv_black_fullscreen = {
    0x2e633cfd,
    0xbb88,
    0x4e69,
    {0xa0, 0xe4, 0x7f, 0x23, 0x2a, 0xdc, 0x5a, 0xd6}};
static const GUID guid_cfg_mpv_stop_hidden = {
    0x9de7e631,
    0x64f8,
    0x4047,
    {0x88, 0x39, 0x8f, 0x4a, 0x50, 0xa0, 0xb7, 0x2f}};

static cfg_bool cfg_mpv_black_fullscreen(g_guid_cfg_mpv_black_fullscreen, true);
static cfg_string cfg_mpv_popup_titleformat(
    g_guid_cfg_mpv_popup_titleformat, "%title% - %artist%[' ('%album%')']");
static cfg_uint cfg_mpv_bg_color(g_guid_cfg_mpv_bg_color, 0);
static cfg_bool cfg_mpv_stop_hidden(guid_cfg_mpv_stop_hidden, true);

static titleformat_object::ptr popup_titlefomat_script;

static const GUID guid_cfg_mpv_branch = {
    0xa8d3b2ca,
    0xa9a,
    0x4efc,
    {0xa4, 0x33, 0x32, 0x4d, 0x76, 0xcc, 0x8a, 0x33}};

static const GUID guid_cfg_mpv_max_drift = {
    0x69ee4b45,
    0x9688,
    0x45e8,
    {0x89, 0x44, 0x8a, 0xb0, 0x92, 0xd6, 0xf3, 0xf8}};
static const GUID guid_cfg_mpv_hard_sync_interval = {
    0xa095f426,
    0x3df7,
    0x434e,
    {0x91, 0x13, 0x19, 0x1a, 0x1b, 0x5f, 0xc1, 0xe5}};
static const GUID guid_cfg_mpv_hard_sync = {
    0xeffb3f43,
    0x60a0,
    0x4a2f,
    {0xbd, 0x78, 0x27, 0x43, 0xa1, 0x6d, 0xd2, 0xbb}};

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

static advconfig_branch_factory g_mpv_branch(
    "mpv", guid_cfg_mpv_branch, advconfig_branch::guid_branch_playback, 0);
static advconfig_integer_factory cfg_mpv_max_drift(
    "Permitted timing drift (ms)", guid_cfg_mpv_max_drift, guid_cfg_mpv_branch,
    0, 20, 0, 1000, 0);
static advconfig_integer_factory cfg_mpv_hard_sync("Hard sync threshold (ms)",
                                                   guid_cfg_mpv_hard_sync,
                                                   guid_cfg_mpv_branch, 0, 2000,
                                                   0, 10000, 0);
static advconfig_integer_factory cfg_mpv_hard_sync_interval(
    "Minimum time between hard syncs (seconds)",
    guid_cfg_mpv_hard_sync_interval, guid_cfg_mpv_branch, 0, 10, 0, 30, 0);
static advconfig_checkbox_factory cfg_mpv_logging(
    "Enable verbose console logging", guid_cfg_mpv_logging, guid_cfg_mpv_branch,
    0, false);
static advconfig_checkbox_factory cfg_mpv_native_logging(
    "Enable mpv log file", guid_cfg_mpv_native_logging, guid_cfg_mpv_branch, 0,
    false);

static const GUID guid_cfg_mpv_video_enabled = {
    0xe3a285f2,
    0x6804,
    0x4291,
    {0xa6, 0x8d, 0xb6, 0xac, 0x41, 0x89, 0x8c, 0x1d}};

static cfg_bool cfg_mpv_video_enabled(guid_cfg_mpv_video_enabled, true);

static std::vector<mpv_container*> g_mpv_containers;
static std::unique_ptr<mpv_player> g_mpv_player;

static void invalidate_all_containers() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).invalidate();
  }
}

bool mpv_container::container_is_on() {
  return g_mpv_player != NULL && g_mpv_player->contained_in(this);
}

void mpv_container::update_player_window() {
  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

bool mpv_container::container_is_pinned() { return pinned; }

t_ui_color mpv_container::get_bg() { return cfg_mpv_bg_color; }

void get_popup_title(pfc::string8& s) {
  if (popup_titlefomat_script.is_empty()) {
    static_api_ptr_t<titleformat_compiler>()->compile_safe(
        popup_titlefomat_script, cfg_mpv_popup_titleformat);
  }

  playback_control::get()->playback_format_title(
      NULL, s, popup_titlefomat_script, NULL,
      playback_control::t_display_level::display_level_all);
}

void mpv_container::container_unpin() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

void mpv_container::container_pin() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  pinned = true;

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

void mpv_container::container_on_context_menu(CWindow wnd, CPoint point) {
  try {
    {
      // handle the context menu key case - center the menu
      if (point == CPoint(-1, -1)) {
        CRect rc;
        WIN32_OP(wnd.GetWindowRect(&rc));
        point = rc.CenterPoint();
      }

      CMenuDescriptionHybrid menudesc(container_wnd());

      static_api_ptr_t<contextmenu_manager> api;
      CMenu menu;
      WIN32_OP(menu.CreatePopupMenu());

      if (g_mpv_player) {
        g_mpv_player->add_menu_items(&menu, &menudesc);
      }

      add_menu_items(&menu, &menudesc);

      int cmd =
          menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                              point.x, point.y, menudesc, 0);

      if (g_mpv_player) {
        g_mpv_player->handle_menu_cmd(cmd);
      }
      handle_menu_cmd(cmd);
    }
  } catch (std::exception const& e) {
    console::complain("Context menu failure", e);
  }
}

void mpv_container::container_toggle_fullscreen() {
  if (g_mpv_player) {
    g_mpv_player->toggle_fullscreen();
  }
}

void mpv_container::container_resize(long p_x, long p_y) {
  cx = p_x;
  cy = p_y;
  update_player_window();
}

void mpv_container::container_create() {
  g_mpv_containers.push_back(this);

  if (!g_mpv_player) {
    g_mpv_player = std::unique_ptr<mpv_player>(new mpv_player());
  } else {
    g_mpv_player->update();
  }
}

void mpv_container::container_destroy() {
  g_mpv_containers.erase(
      std::remove(g_mpv_containers.begin(), g_mpv_containers.end(), this),
      g_mpv_containers.end());

  if (g_mpv_containers.empty()) {
    if (g_mpv_player != NULL) g_mpv_player->destroy();
    g_mpv_player = NULL;
  }

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

mpv_player::mpv_player()
    : wid(NULL),
      enabled(false),
      mpv(NULL),
      sync_task(sync_task_type::Wait),
      sync_on_unpause(false),
      last_mpv_seek(0),
      time_base(0),
      mpv_loaded(load_mpv()) {
  update_container();
  mpv_set_wid(Create(container->container_wnd(), 0, 0, WS_CHILD,
                     WS_EX_TRANSPARENT | WS_EX_LAYERED));
  update_window();

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

void mpv_player::destroy() { DestroyWindow(); }

BOOL mpv_player::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(cfg_mpv_bg_color) != NULL);
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
  if (is_mpv_loaded()) {
    if (check_for_idle()) {
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

      menu->AppendMenu(MF_SEPARATOR, ID_STATS, _T(""));
    }
  }

  menu->AppendMenu(cfg_mpv_video_enabled ? MF_CHECKED : MF_UNCHECKED,
                   ID_ENABLED, _T("Enabled"));
  menudesc->Set(ID_ENABLED, "Enable/disable video playback");
  menu->AppendMenu(fullscreen_ ? MF_CHECKED : MF_UNCHECKED, ID_FULLSCREEN,
                   _T("Fullscreen"));
  menudesc->Set(ID_FULLSCREEN, "Toggle video fullscreen");
}

void mpv_player::handle_menu_cmd(int cmd) {
  switch (cmd) {
    case ID_ENABLED:
      cfg_mpv_video_enabled = !cfg_mpv_video_enabled;
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
    SetWindowLong(GWL_EXSTYLE,
                  saved_ex_style & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));

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
  ShowWindow(SW_SHOW);
  return 0;
}

void mpv_player::update_container() {
  double top_priority = -1.0;
  mpv_container* main = NULL;
  bool keep = false;
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    if ((*it) == container) {
      keep = true;
    }
    if ((*it)->container_is_pinned() ||
        ((*it)->is_popup() && (main == NULL || !main->container_is_pinned()))) {
      main = (*it);
      break;
    }
    if (!(*it)->is_visible()) continue;

    double priority = (*it)->cx * (*it)->cy;
    if (priority > top_priority) {
      main = *it;
      top_priority = priority;
      continue;
    }
  }

  if (main != NULL) {
    container = main;
  } else if (!keep) {
    container = *g_mpv_containers.begin();
  }
}

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
  if (cfg_mpv_video_enabled &&
      (fullscreen_ || container->is_visible() || !cfg_mpv_stop_hidden)) {
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

  if (!mpv_loaded || mpv == NULL) return;

  set_background();
}

bool mpv_player::contained_in(mpv_container* p_container) {
  return container == p_container;
}

void mpv_player::set_background() {
  std::stringstream colorstrings;
  colorstrings << "#";
  t_uint32 bgcolor = fullscreen_ && cfg_mpv_black_fullscreen
                         ? 0
                         : cfg_mpv_bg_color.get_value();
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetRValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetGValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetBValue(bgcolor);
  std::string colorstring = colorstrings.str();
  _mpv_set_option_string(mpv, "background", colorstring.c_str());
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

    set_background();

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
  mpv_seek(p_time, true);
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

void mpv_player::update_title() {
  pfc::string8 title;
  mpv::get_popup_title(title);
  uSetWindowText(m_hWnd, title);
}

void mpv_player::mpv_play(metadb_handle_ptr metadb, bool new_file) {
  if (!mpv_loaded) return;

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

static abort_callback_impl cb;
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

  if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
      cfg_mpv_logging.get()) {
    console::error("mpv: Error pausing");
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
}  // namespace mpv

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
  double vis_time = 0.0;
  visualisation_stream::ptr vis_stream = NULL;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  if (!vis_stream.is_valid()) {
    console::error(
        "mpv: Video disabled: this output has no timing information");
    fb2k::inMainThread([this]() {
      cfg_mpv_video_enabled = false;
      update();
    });
    return;
  }
  vis_stream->get_absolute_time(vis_time);

  if (cfg_mpv_logging.get()) {
    msg.str("");
    msg << "mpv: Audio time "
        << time_base + g_timing_info.load().last_fb_seek + vis_time -
               g_timing_info.load().last_seek_vistime;
    console::info(msg.str().c_str());
  }

  int count = 0;
  while (time_base + g_timing_info.load().last_fb_seek + vis_time -
             g_timing_info.load().last_seek_vistime <
         mpv_time) {
    if (sync_task != sync_task_type::FirstFrameSync) {
      return;
    }
    Sleep(10);
    vis_stream->get_absolute_time(vis_time);
    if (count++ > 500 && !vis_stream->get_absolute_time(vis_time)) {
      console::error(
          "mpv: Initial seek failed, maybe this output does not have accurate "
          "timing info");
      if (_mpv_set_property_string(mpv, "pause", "no") < 0 &&
          cfg_mpv_logging.get()) {
        console::error("mpv: Error pausing");
      }
      return;
    }
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

const char* mpv_player::get_string(const char* name) {
  if (!mpv_loaded || mpv == NULL) return "Error";
  return _mpv_get_property_string(mpv, name);
}

bool mpv_player::is_mpv_loaded() { return mpv_loaded && mpv != NULL; }

bool mpv_player::get_bool(const char* name) {
  if (!mpv_loaded || mpv == NULL) return false;
  int flag = 0;
  _mpv_get_property(mpv, name, MPV_FORMAT_FLAG, &flag);
  return flag == 1;
}

double mpv_player::get_double(const char* name) {
  if (!mpv_loaded || mpv == NULL) return 0;
  double num = 0;
  _mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &num);
  return num;
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

class CMpvPreferences : public CDialogImpl<CMpvPreferences>,
                        public preferences_page_instance {
 public:
  CMpvPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback),
        button_brush(CreateSolidBrush(cfg_mpv_bg_color)) {}

  enum { IDD = IDD_MPV_PREFS };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMyPreferences)
  MSG_WM_INITDIALOG(OnInitDialog)
  MSG_WM_CTLCOLORBTN(on_color_button)
  COMMAND_HANDLER_EX(IDC_BUTTON_BG, BN_CLICKED, OnBgClick);
  COMMAND_HANDLER_EX(IDC_CHECK_FSBG, EN_CHANGE, OnEditChange)
  COMMAND_HANDLER_EX(IDC_CHECK_STOP, EN_CHANGE, OnEditChange)
  COMMAND_HANDLER_EX(IDC_EDIT_POPUP, EN_CHANGE, OnEditTextChange)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnBgClick(UINT, int, CWindow);
  void OnEditChange(UINT, int, CWindow);
  void OnEditTextChange(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  CBrush button_brush;
  HBRUSH on_color_button(HDC wp, HWND lp);
  bool edit_dirty = false;

  const preferences_page_callback::ptr m_callback;

  COLORREF bg_col = 0;
};

HBRUSH CMpvPreferences::on_color_button(HDC wp, HWND lp) {
  if (lp == GetDlgItem(IDC_BUTTON_BG)) {
    return button_brush;
  }
  return NULL;
}

BOOL CMpvPreferences::OnInitDialog(CWindow, LPARAM) {
  bg_col = cfg_mpv_bg_color.get_value();
  button_brush = CreateSolidBrush(bg_col);

  CheckDlgButton(IDC_CHECK_FSBG, cfg_mpv_black_fullscreen);
  CheckDlgButton(IDC_CHECK_STOP, cfg_mpv_stop_hidden);

  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_mpv_popup_titleformat);

  edit_dirty = false;

  return FALSE;
}

void CMpvPreferences::OnBgClick(UINT, int, CWindow) {
  CHOOSECOLOR cc = {};
  static COLORREF acrCustClr[16];
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = get_wnd();
  cc.lpCustColors = (LPDWORD)acrCustClr;
  cc.rgbResult = bg_col;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;

  if (ChooseColor(&cc) == TRUE) {
    bg_col = cc.rgbResult;
    button_brush = CreateSolidBrush(bg_col);
    OnChanged();
  }
}

void CMpvPreferences::OnEditChange(UINT, int, CWindow) { OnChanged(); }
void CMpvPreferences::OnEditTextChange(UINT, int, CWindow) {
  edit_dirty = true;
  OnChanged();
}

t_uint32 CMpvPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvPreferences::reset() {
  bg_col = 0;
  button_brush = CreateSolidBrush(bg_col);
  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, "%title% - %artist%[ (%album%)]");
  CheckDlgButton(IDC_CHECK_FSBG, true);
  CheckDlgButton(IDC_CHECK_STOP, true);
  OnChanged();
}

void CMpvPreferences::apply() {
  cfg_mpv_bg_color = bg_col;

  cfg_mpv_black_fullscreen = IsDlgButtonChecked(IDC_CHECK_FSBG);
  cfg_mpv_stop_hidden = IsDlgButtonChecked(IDC_CHECK_STOP);

  auto length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_POPUP));
  pfc::string format = uGetDlgItemText(m_hWnd, IDC_EDIT_POPUP);
  cfg_mpv_popup_titleformat.reset();
  cfg_mpv_popup_titleformat.set_string(format.get_ptr());

  static_api_ptr_t<titleformat_compiler>()->compile_safe(
      popup_titlefomat_script, cfg_mpv_popup_titleformat);
  invalidate_all_containers();

  edit_dirty = false;

  OnChanged();
}

bool CMpvPreferences::HasChanged() {
  auto length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_POPUP));
  WCHAR* buf = new WCHAR[length + 1];
  GetDlgItemTextW(IDC_EDIT_POPUP, buf, length + 1);
  pfc::string8 str;
  str << buf;
  return edit_dirty ||
         IsDlgButtonChecked(IDC_CHECK_FSBG) != cfg_mpv_black_fullscreen ||
         IsDlgButtonChecked(IDC_CHECK_STOP) != cfg_mpv_stop_hidden ||
         bg_col != cfg_mpv_bg_color;
}

void CMpvPreferences::OnChanged() {
  m_callback->on_state_changed();
  Invalidate();
}

class preferences_page_mpv_impl
    : public preferences_page_impl<CMpvPreferences> {
 public:
  const char* get_name() { return "mpv"; }
  GUID get_guid() {
    static const GUID guid = {0x11c90957,
                              0xf691,
                              0x4c23,
                              {0xb5, 0x87, 0x8, 0x9e, 0x5d, 0xfa, 0x14, 0x7a}};

    return guid;
  }
  GUID get_parent_guid() { return guid_tools; }
};

static preferences_page_factory_t<preferences_page_mpv_impl>
    g_preferences_page_mpv_impl_factory;
}  // namespace mpv