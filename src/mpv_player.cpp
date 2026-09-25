#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <thread>

#include "../helpers/VolumeMap.h"
#include "../helpers/atl-misc.h"
#include "../helpers/win32_misc.h"
#include "artwork_protocol.h"
#include "menu_utils.h"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"
#include "timing_info.h"

void RunMpvFullscreenWindow(bool reopen_popup, MONITORINFO monitor);

namespace mpv {

static CWindowAutoLifetime<mpv_player>* g_player;

void mpv_player::on_containers_change() {
  auto main = mpv_container::get_main_container();
  if (main && !g_player) {
    g_player = new CWindowAutoLifetime<mpv_player>(main->container_wnd());
    main->on_gain_player();
  }

  if (g_player) g_player->update();
}

void mpv_player::restart() {
  if (g_player) {
    auto main = mpv_container::get_main_container();
    g_player->DestroyWindow();
    g_player = new CWindowAutoLifetime<mpv_player>(main->container_wnd());
    main->on_gain_player();
  }
}

void mpv_player::get_title(pfc::string8& out) {
  if (g_player) {
    std::lock_guard<std::mutex> lock(g_player->display_item_mutex);
    if (g_player->current_display_item.is_valid()) {
      format_player_title(out, g_player->current_display_item);
    }
  }
}

void mpv_player::toggle_fullscreen() {
  if (g_player) {
    g_player->container->toggle_fullscreen();
  } else {
    MONITORINFO monitor;
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(core_api::get_main_window(),
                                      MONITOR_DEFAULTTONEAREST),
                    &monitor);

    RunMpvFullscreenWindow(false, monitor);
  }
}

struct monitor_result {
  HMONITOR hmon;
  unsigned count;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
                                     LPRECT lprcMonitor, LPARAM dwData) {
  monitor_result* res = reinterpret_cast<monitor_result*>(dwData);
  if (res->count == 0) {
    res->hmon = hMonitor;
    return FALSE;
  }
  --res->count;
  return TRUE;
}

void mpv_player::fullscreen_on_monitor(int monitor) {
  if (monitor < 0) return;

  monitor_result r;
  r.hmon = NULL;
  r.count = static_cast<unsigned>(monitor);
  EnumDisplayMonitors(NULL, NULL, MonitorEnumProc,
                      reinterpret_cast<LPARAM>(&r));
  if (r.hmon != NULL) {
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(r.hmon, &monitor_info);
    RunMpvFullscreenWindow(false, monitor_info);
  }
}

static const int time_pos_userdata = 28903278;
static const int seeking_userdata = 982628764;
static const int path_userdata = 982628764;
static const int idle_active_userdata = 12792384;

extern cfg_bool cfg_video_enabled, cfg_black_fullscreen, cfg_stop_hidden,
    cfg_artwork, cfg_osc, cfg_osc_scalewithvideo, cfg_hwdec, cfg_latency,
    cfg_deint, cfg_gpuhq;
extern cfg_uint cfg_bg_color, cfg_artwork_type, cfg_osc_layout,
    cfg_osc_seekbarstyle, cfg_osc_transparency, cfg_osc_fadeduration,
    cfg_osc_deadzone, cfg_osc_scalewindowed, cfg_osc_scalefullscreen,
    cfg_osc_timeout;
extern advconfig_checkbox_factory cfg_logging, cfg_mpv_logfile, cfg_foo_youtube,
    cfg_ytdl_any, cfg_remote_always_play;
extern advconfig_integer_factory cfg_max_drift, cfg_hard_sync_threshold,
    cfg_hard_sync_interval, cfg_seek_seconds, cfg_remote_offset;

mpv_player::mpv_player()
    : enabled(false),
      mpv_handle(NULL),
      mpv_window_hwnd(NULL),
      task_queue(),
      sync_on_unpause(false),
      last_mpv_seek(0),
      last_hard_sync(-99),
      last_sync_time(0),
      running_ffs(false),
      current_display_item(NULL),
      mpv_timepos(0),
      mpv_state(state::Unloaded),
      container(mpv_container::get_main_container()),
      time_base(0) {
  metadb_handle_list selection;
  ui_selection_manager::get()->get_selection(selection);
  on_selection_changed(selection);

  control_thread = std::thread([this]() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex);
      control_thread_cv.wait(lock, [this] { return !task_queue.empty(); });
      task task = task_queue.front();
      task_queue.pop_front();
      lock.unlock();

      switch (task.type) {
        case task_type::Quit:
          {
            std::lock_guard<std::recursive_mutex> mpv_lock(mpv_mutex);
            if (mpv_handle) {
              libmpv::get()->terminate_destroy(mpv_handle);
              mpv_handle = nullptr;
              mpv_loaded = false;
            }
          }
          return;
        case task_type::Play:
          play(task.play_file, task.time);
          break;
        case task_type::Stop:
          stop();
          break;
        case task_type::Seek:
          seek(task.time, task.flag);
          break;
        case task_type::FirstFrameSync:
          running_ffs = true;
          initial_sync();
          running_ffs = false;
          break;
        case task_type::Pause:
          pause(task.flag);
          break;
        case task_type::LoadArtwork:
          load_artwork();
          break;
        case task_type::Command:
          run_command(task.arguments);
          break;
        case task_type::Sync:
          sync(task.time, task.flag, task.sampled_at);
          break;
        case task_type::HideCursorForMenu:
          cursor_autohide_before_menu = get_string("cursor-autohide");
          if (cursor_autohide_before_menu.is_empty())
            cursor_autohide_before_menu = "1000";
          set_property_string("cursor-autohide", "no");
          break;
        case task_type::RestoreCursorAfterMenu:
          set_property_string("cursor-autohide",
                              cursor_autohide_before_menu.c_str());
          break;
        case task_type::RefreshMediaInfo:
          refresh_media_info();
          break;
      }
    }
  });
}

mpv_player::~mpv_player() {
  {
    std::lock_guard<std::mutex> init_lock(init_mutex);
    event_listener_stop = true;
    if (mpv_loaded) libmpv::get()->wakeup(mpv_handle);
    if (event_listener.joinable()) event_listener.join();
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    while (!task_queue.empty()) task_queue.pop_front();
    task t;
    t.type = task_type::Quit;
    task_queue.push_back(t);
  }
  control_thread_cv.notify_all();
  event_cv.notify_all();

  if (control_thread.joinable()) control_thread.join();
  g_player = NULL;
}

bool mpv_player::check_queue_any() {
  for (const auto& queued : task_queue) {
    switch (queued.type) {
      case task_type::Quit:
      case task_type::Play:
      case task_type::Seek:
      case task_type::Pause:
      case task_type::Stop:
      case task_type::LoadArtwork:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool mpv_player::check_queue_time_change_locking() {
  std::lock_guard<std::mutex> lock(mutex);
  for (auto& task : task_queue) {
    if (task.type == task_type::Seek || task.type == task_type::Play ||
        task.type == task_type::Quit || task.type == task_type::Stop) {
      return true;
    }
  }

  return false;
}

void mpv_player::queue_task(task t) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (t.type == task_type::Sync) {
      for (auto it = task_queue.begin(); it != task_queue.end();) {
        if (it->type == task_type::Sync)
          it = task_queue.erase(it);
        else
          ++it;
      }
    }
    task_queue.push_back(t);
  }
  control_thread_cv.notify_all();
  event_cv.notify_all();
}

void mpv_player::queue_command(
    std::initializer_list<std::string> arguments) {
  task t;
  t.type = task_type::Command;
  t.arguments.assign(arguments.begin(), arguments.end());
  queue_task(std::move(t));
}

void mpv_player::run_command(const std::vector<std::string>& arguments) {
  if (arguments.empty()) return;

  std::vector<const char*> command_arguments;
  command_arguments.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    command_arguments.push_back(argument.c_str());
  }
  command_arguments.push_back(nullptr);
  const int result = command(command_arguments.data());
  if (result < 0 && cfg_logging) {
    pfc::string_formatter description;
    for (const auto& argument : arguments) {
      if (!description.is_empty()) description << " ";
      description << argument.c_str();
    }
    FB2K_console_formatter() << "mpv: Queued command failed (" << result
                            << "): " << description;
  }
}

void mpv_player::refresh_media_info() {
  pfc::string8 codec_info;
  codec_info << get_string("video-codec") << " "
             << get_string("video-params/pixelformat");

  pfc::string8 display_info;
  display_info << get_string("width") << "x" << get_string("height") << " "
               << pfc::format_float(get_double("container-fps"), 0, 3)
               << "fps (display "
               << pfc::format_float(get_double("estimated-vf-fps"), 0, 3)
               << "fps)";

  pfc::string8 hwdec_info = get_string("hwdec-current");
  if (!hwdec_info.equals("no") && !hwdec_info.is_empty())
    hwdec_info.insert_chars(0, "Hardware decoding: ");
  else
    hwdec_info.reset();

  std::lock_guard<std::mutex> lock(published_state_mutex);
  media_codec_info = codec_info;
  media_display_info = display_info;
  media_hwdec_info = hwdec_info;
}

void mpv_player::set_state(state state) {
  if (cfg_logging) {
    switch (state) {
      case state::Active:
        FB2K_console_formatter() << "mpv: State -> Active";
        break;
      case state::Artwork:
        FB2K_console_formatter() << "mpv: State -> Artwork";
        break;
      case state::Idle:
        FB2K_console_formatter() << "mpv: State -> Idle";
        break;
      case state::Loading:
        FB2K_console_formatter() << "mpv: State -> Loading";
        break;
      case state::Preload:
        FB2K_console_formatter() << "mpv: State -> Preload";
        break;
      case state::Seeking:
        FB2K_console_formatter() << "mpv: State -> Seeking";
        break;
      case state::Shutdown:
        FB2K_console_formatter() << "mpv: State -> Shutdown";
        break;
      case state::Unloaded:
        FB2K_console_formatter() << "mpv: State -> Unloaded";
        break;
    }
  }

  mpv_state = state;
}

BOOL mpv_player::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(
                 background_color_from_config(cfg_bg_color)) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}

void mpv_player::add_menu_items(uie::menu_hook_impl& menu_hook) {
  if (g_player && g_player->mpv_loaded) {
    if (g_player->mpv_state == state::Idle ||
        g_player->mpv_state == state::Artwork ||
        g_player->mpv_state == state::Unloaded) {
      menu_hook.add_node(new menu_utils::menu_node_disabled("Idle"));
    } else if (g_player->mpv_state == state::Loading ||
               g_player->mpv_state == state::Preload) {
      menu_hook.add_node(new menu_utils::menu_node_disabled("Loading..."));
    } else {
      pfc::string8 codec_info;
      pfc::string8 display_info;
      pfc::string8 hwdec_info;
      {
        std::lock_guard<std::mutex> lock(g_player->published_state_mutex);
        codec_info = g_player->media_codec_info;
        display_info = g_player->media_display_info;
        hwdec_info = g_player->media_hwdec_info;
      }
      menu_hook.add_node(new menu_utils::menu_node_disabled(codec_info));
      menu_hook.add_node(new menu_utils::menu_node_disabled(display_info));
      if (!hwdec_info.is_empty())
        menu_hook.add_node(new menu_utils::menu_node_disabled(hwdec_info));
    }

    menu_hook.add_node(new uie::menu_node_separator_t());

    std::vector<pfc::string8> profiles;
    {
      std::lock_guard<std::mutex> lock(g_player->published_state_mutex);
      profiles = g_player->profiles;
    }
    if (!profiles.empty()) {
      std::vector<ui_extension::menu_node_ptr> profile_children;
      for (auto& profile : profiles) {
        profile_children.emplace_back(
            new menu_utils::menu_node_run(profile, false, [profile]() {
              if (g_player)
                g_player->queue_command(
                    {"apply-profile", profile.c_str()});
            }));
      }
      menu_hook.add_node(
          new menu_utils::menu_node_popup("Profile", profile_children));

      menu_hook.add_node(new uie::menu_node_separator_t());
    }

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
  } else {
    menu_hook.add_node(new menu_utils::menu_node_disabled("Unloaded"));
  }

  if (cfg_artwork &&
      (!g_player || !g_player->mpv_loaded || g_player->mpv_state == state::Idle ||
       g_player->mpv_state == state::Artwork)) {
    menu_hook.add_node(new uie::menu_node_separator_t());

    const auto selected = artwork_type_from_config(cfg_artwork_type);
    std::vector<ui_extension::menu_node_ptr> artwork_children;
    artwork_children.emplace_back(
        new menu_utils::menu_node_run("Front", selected == artwork_type::Front, []() {
          cfg_artwork_type = static_cast<unsigned>(artwork_type::Front);
          reload_artwork();
        }));
    artwork_children.emplace_back(
        new menu_utils::menu_node_run("Back", selected == artwork_type::Back, []() {
          cfg_artwork_type = static_cast<unsigned>(artwork_type::Back);
          reload_artwork();
        }));
    artwork_children.emplace_back(
        new menu_utils::menu_node_run("Disc", selected == artwork_type::Disc, []() {
          cfg_artwork_type = static_cast<unsigned>(artwork_type::Disc);
          reload_artwork();
        }));
    artwork_children.emplace_back(
        new menu_utils::menu_node_run("Artist", selected == artwork_type::Artist, []() {
          cfg_artwork_type = static_cast<unsigned>(artwork_type::Artist);
          reload_artwork();
        }));

    menu_hook.add_node(
        new menu_utils::menu_node_popup("Cover type", artwork_children));
  }
}

void mpv_player::on_context_menu(CWindow wnd, CPoint point) {
  task hide_cursor;
  hide_cursor.type = task_type::HideCursorForMenu;
  queue_task(std::move(hide_cursor));
  container->on_context_menu(wnd, point);
  task restore_cursor;
  restore_cursor.type = task_type::RestoreCursorAfterMenu;
  queue_task(std::move(restore_cursor));
}

void mpv_player::on_destroy() {}

LRESULT mpv_player::on_create(LPCREATESTRUCT lpcreate) {
  update();
  return 0;
}

void mpv_player::on_mouse_move(UINT, CPoint point) {
  if (!mpv_loaded) return;
  find_window();
}

void mpv_player::send_message(UINT msg, UINT wp, UINT lp) {
  if (g_player && g_player->find_window()) {
    SendMessage(g_player->mpv_window_hwnd, msg, wp, lp);
  }
}

bool mpv_player::find_window() {
  if (!mpv_window_hwnd) {
    mpv_window_hwnd = FindWindowEx(m_hWnd, NULL, L"mpv", NULL);
  }
  if (mpv_window_hwnd) {
    ::EnableWindow(mpv_window_hwnd, 1);
  }

  return mpv_window_hwnd;
}

void mpv_player::update() {
  mpv_container* new_container = mpv_container::get_main_container();
  mpv_container* old_container = container;
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
      mpv_container::invalidate_all_containers();
    }
  }

  if (old_container != new_container && old_container) {
    old_container->on_lose_player();
  }

  if (old_container != new_container && new_container) {
    new_container->on_gain_player();
  }

  if (mpv_loaded) {
    queue_command({"script-message", "foobar", "osc-enabled-changed",
                   cfg_osc && container->is_osc_enabled() ? "yes" : "no"});
    queue_command({"set", "fullscreen",
                   container->is_fullscreen() ? "yes" : "no"});
  }

  bool vis = container->is_visible();
  if (cfg_video_enabled && (container->is_fullscreen() ||
                            container->is_visible() || !cfg_stop_hidden)) {
    bool starting = !enabled;
    enabled = true;

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
    bool stopping = enabled;
    enabled = false;

    if (stopping && mpv_state != state::Artwork) {
      task t;
      t.type = task_type::Stop;
      queue_task(t);
    }
  }

  set_background();
}

bool mpv_player::contained_in(mpv_container* p_container) {
  return container == p_container;
}

void mpv_player::update_title() {
  pfc::string8 title;
  {
    std::lock_guard<std::mutex> lock(display_item_mutex);
    format_player_title(title, current_display_item);
  }

  queue_command(
      {"script-message", "foobar", "title-changed", title.c_str()});
  uSetWindowText(m_hWnd, title);
}

std::string mpv_player::get_background_color() {
  std::stringstream colorstrings;
  colorstrings << "#";
  const COLORREF bgcolor = container->is_fullscreen() && cfg_black_fullscreen
                               ? 0
                               : background_color_from_config(cfg_bg_color);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetRValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetGValue(bgcolor);
  colorstrings << std::setfill('0') << std::setw(2) << std::hex
               << (unsigned)GetBValue(bgcolor);
  return colorstrings.str();
}

void mpv_player::set_background() {
  if (mpv_loaded)
    queue_command({"set", "background-color", get_background_color()});
}

bool mpv_player::mpv_init() {
  if (!libmpv::get()->ready) return false;
  std::lock_guard<std::mutex> lock_init(init_mutex);

  if (!mpv_handle && m_hWnd && container) {
    pfc::string_formatter path;
    filesystem::g_get_native_path(core_api::get_profile_path(), path);
    path.add_filename("mpv");
    mpv_handle = libmpv::get()->create();
    if (!mpv_handle) {
      FB2K_console_formatter() << "mpv: Could not create libmpv context";
      return false;
    }

    int64_t l_wid = (intptr_t)(m_hWnd);
    set_option("wid", libmpv::MPV_FORMAT_INT64, &l_wid);

    set_option_string("config", "yes");
    set_option_string("config-dir", path.c_str());

    set_option_string("ytdl", "yes");
    set_option_string("ytdl-format", "bestvideo/best");
    set_option_string("osc", "no");
    set_option_string("load-stats-overlay", "yes");
    set_option_string("load-scripts", "yes");
    set_option_string("alpha", "blend");

    const std::string background = get_background_color();
    set_option_string("background", "color");
    set_option_string("background-color", background.c_str());

    if (cfg_mpv_logfile) {
      path.add_filename("mpv.log");
      set_option_string("log-file", path.c_str());
    }

    // no display for music
    set_option_string("audio-display", "no");

    // everything syncs to foobar
    set_option_string("video-sync", "audio");
    set_option_string("untimed", "no");

    // seek fast
    set_option_string("hr-seek-framedrop", "yes");
    set_option_string("hr-seek-demuxer-offset", "0");
    set_option_string("initial-audio-sync", "no");

    // input
    set_option_string("window-dragging", "no");
    set_option_string("stop-screensaver", "no");

    // foobar plays the audio
    set_option_string("audio", "no");

    // keep the renderer initialised
    set_option_string("force-window", "yes");
    set_option_string("idle", "yes");

    // don't unload the file when finished, maybe fb is still playing and we
    // could be asked to seek backwards
    set_option_string("keep-open", "yes");
    set_option_string("keep-open-pause", "no");
    set_option_string("cache-pause", "no");

    // user settings
    if (cfg_deint) {
      set_option_string("vf-append", "@foompvdeint:bwdif:deint=1");
    }

    if (cfg_hwdec) {
      set_option_string("hwdec", "auto-safe");
    }

    // load the OSC if no custom one is set
    pfc::string_formatter custom_osc_path;
    filesystem::g_get_native_path(core_api::get_profile_path(),
                                  custom_osc_path);
    custom_osc_path.add_filename("mpv");
    custom_osc_path.add_filename("scripts");
    custom_osc_path.add_filename("osc.lua");
    if (!filesystem::g_exists(custom_osc_path.c_str(), fb2k::noAbort)) {
      pfc::string_formatter osc_path = core_api::get_my_full_path();
      osc_path.truncate(osc_path.scan_filename());
      osc_path << "mpv\\osc.lua";
      osc_path.replace_char('\\', '/');
      set_option_string("scripts", osc_path.c_str());
    }

    // apply OSC settings
    std::stringstream opts;
    opts << "osc-layout=";
    switch (config_choice_or_default(cfg_osc_layout, 4, 0)) {
      case 0:
        opts << "bottombar";
        break;
      case 1:
        opts << "topbar";
        break;
      case 2:
        opts << "box";
        break;
      case 3:
        opts << "slimbox";
        break;
    }

    opts << ",osc-seekbarstyle=";
    switch (config_choice_or_default(cfg_osc_seekbarstyle, 3, 0)) {
      case 0:
        opts << "bar";
        break;
      case 1:
        opts << "diamond";
        break;
      case 2:
        opts << "knob";
        break;
    }

    opts << ",osc-boxalpha=" << ((255 * cfg_osc_transparency) / 100);
    opts << ",osc-hidetimeout=" << cfg_osc_timeout;
    opts << ",osc-fadeduration=" << cfg_osc_fadeduration;
    opts << ",osc-deadzonesize=" << (0.01 * cfg_osc_deadzone);
    opts << ",osc-scalewindowed=" << (0.01 * cfg_osc_scalewindowed);
    opts << ",osc-scalefullscreen=" << (0.01 * cfg_osc_scalefullscreen);
    opts << ",osc-vidscale=" << (cfg_osc_scalewithvideo ? "yes" : "no");

    std::string opts_str = opts.str();
    set_option_string("script-opts", opts_str.c_str());

    libmpv::get()->stream_cb_add_ro(mpv_handle, "artwork", this,
                                    artwork_protocol_open);
    set_option_string("image-display-duration", "inf");

    libmpv::get()->observe_property(mpv_handle, seeking_userdata, "seeking",
                                    libmpv::MPV_FORMAT_FLAG);
    libmpv::get()->observe_property(mpv_handle, idle_active_userdata,
                                    "idle-active", libmpv::MPV_FORMAT_FLAG);
    libmpv::get()->observe_property(mpv_handle, path_userdata, "path",
                                    libmpv::MPV_FORMAT_STRING);

    if (libmpv::get()->initialize(mpv_handle) != 0) {
      libmpv::get()->terminate_destroy(mpv_handle);
      mpv_handle = NULL;
      mpv_loaded = false;
    } else {
      mpv_loaded = true;
      event_listener = std::thread([this]() {
        if (!mpv_handle) {
          FB2K_console_formatter()
              << "mpv: libmpv event listener started but mpv wasn't running";
          return;
        }

        bool event_idle = true;
        bool event_seeking = false;
        std::string event_path;
        while (!event_listener_stop) {
          libmpv::mpv_event* event = libmpv::get()->wait_event(mpv_handle, -1);
          if (event_listener_stop) return;

          if (event->event_id == libmpv::MPV_EVENT_CLIENT_MESSAGE) {
              libmpv::mpv_event_client_message* event_message =
                  (libmpv::mpv_event_client_message*)event->data;
              if (event_message->num_args > 1 &&
                  strcmp(event_message->args[0], "foobar") == 0) {
                if (event_message->num_args > 2 &&
                    strcmp(event_message->args[1], "seek") == 0) {
                  const char* time_str = event_message->args[2];
                  try {
                    double time = std::stod(time_str);
                    fb2k::inMainThread([time]() {
                      playback_control::get()->playback_seek(time);
                    });
                  } catch (...) {
                    FB2K_console_formatter()
                        << "mpv: Could not process seek script-message: "
                           "invalid argument "
                        << time_str << ", ignoring";
                  }
                } else if (event_message->num_args > 2 &&
                           strcmp(event_message->args[1], "context") == 0) {
                  pfc::string8 menu_cmd(event_message->args[2]);
                  for (int i = 3; i < event_message->num_args; i++) {
                    menu_cmd << " " << event_message->args[i];
                  }
                  metadb_handle_ptr display_item;
                  {
                    std::lock_guard<std::mutex> display_lock(
                        display_item_mutex);
                    display_item = current_display_item;
                  }
                  std::weak_ptr<void> lifetime(lifetime_token);
                  fb2k::inMainThread([lifetime, menu_cmd, display_item]() {
                    if (lifetime.expired()) return;
                    metadb_handle_list list;
                    list.add_item(display_item);
                    menu_utils::run_contextmenu_item(menu_cmd, list);
                  });
                } else if (event_message->num_args > 2 &&
                           strcmp(event_message->args[1], "menu") == 0) {
                  pfc::string8 menu_cmd(event_message->args[2]);
                  for (int i = 3; i < event_message->num_args; i++) {
                    menu_cmd << " " << event_message->args[i];
                  }
                  std::weak_ptr<void> lifetime(lifetime_token);
                  fb2k::inMainThread([lifetime, menu_cmd]() {
                    if (lifetime.expired()) return;
                    menu_utils::run_mainmenu_item(menu_cmd);
                  });
                } else if (event_message->num_args > 3 &&
                           strcmp(event_message->args[1],
                                  "register-titleformat") == 0) {
                  pfc::string8 id(event_message->args[2]);
                  pfc::string8 format(event_message->args[3]);
                  for (int i = 4; i < event_message->num_args; i++) {
                    format << " " << event_message->args[i];
                  }
                  std::weak_ptr<void> lifetime(lifetime_token);
                  fb2k::inMainThread([lifetime, id, format]() {
                    if (lifetime.expired() || !g_player) return;
                    titleformat_object::ptr object;
                    if (!titleformat_compiler::get()->compile(object, format))
                      return;
                    titleformat_subscription sub = {id, object};
                    {
                      std::lock_guard<std::mutex> titleformat_lock(
                          g_player->titleformat_mutex);
                      g_player->titleformat_subscriptions.push_back(sub);
                    }
                    g_player->publish_titleformatting_subscriptions();
                  });
                }
              }
            } else if (event->event_id == libmpv::MPV_EVENT_SHUTDOWN) {
              mpv_state = state::Shutdown;
              return;
            } else if (event->event_id == libmpv::MPV_EVENT_PROPERTY_CHANGE) {
              libmpv::mpv_event_property* event_property =
                  (libmpv::mpv_event_property*)event->data;

              if (event->reply_userdata == time_pos_userdata &&
                  event_property->format == libmpv::MPV_FORMAT_DOUBLE &&
                  event_property->data != nullptr) {
                mpv_timepos = *(double*)(event_property->data);
              } else if (event->reply_userdata == seeking_userdata ||
                          event->reply_userdata == idle_active_userdata ||
                          event->reply_userdata == path_userdata) {
                if (event->reply_userdata == seeking_userdata &&
                    event_property->format == libmpv::MPV_FORMAT_FLAG &&
                    event_property->data != nullptr) {
                  event_seeking = *(int*)(event_property->data) != 0;
                } else if (event->reply_userdata == idle_active_userdata &&
                           event_property->format == libmpv::MPV_FORMAT_FLAG &&
                           event_property->data != nullptr) {
                  event_idle = *(int*)(event_property->data) != 0;
                } else if (event->reply_userdata == path_userdata &&
                           event_property->format == libmpv::MPV_FORMAT_STRING) {
                  const char* value = event_property->data == nullptr
                                          ? nullptr
                                          : *(char**)event_property->data;
                  event_path = value == nullptr ? "" : value;
                }

                bool showing_art = event_path == "artwork://";

                state new_state =
                    showing_art ? state::Artwork
                                : event_seeking
                                      ? state::Seeking
                                      : event_idle ? state::Idle : state::Active;

                if (mpv_state == state::Preload && new_state == state::Idle) {
                  new_state = state::Preload;
                }

                const state previous_state = mpv_state;
                if (previous_state != new_state) {
                  // Publish Idle before scheduling the main-thread callback:
                  // it may run immediately and checks the current state.
                  set_state(new_state);
                  if (new_state == state::Idle &&
                      previous_state != state::Artwork) {
                    std::weak_ptr<void> lifetime(lifetime_token);
                    fb2k::inMainThread([lifetime]() {
                      if (lifetime.expired() || !g_player ||
                          g_player->mpv_state != state::Idle)
                        return;
                      request_artwork();
                    });
                  }

                  if (new_state == state::Active) {
                    task refresh;
                    refresh.type = task_type::RefreshMediaInfo;
                    queue_task(std::move(refresh));
                  }
                }
              }
          }

          control_thread_cv.notify_all();
          event_cv.notify_all();
        }
      });

      if (cfg_gpuhq) {
        const char* cmd_profile[] = {"apply-profile", "gpu-hq", NULL};
        if (command(cmd_profile) < 0 && cfg_logging) {
          FB2K_console_formatter() << "mpv: Error applying gpu-hq profile";
        }
      }

      if (cfg_latency) {
        const char* cmd_profile[] = {"apply-profile", "low-latency", NULL};
        if (command(cmd_profile) < 0 && cfg_logging) {
          FB2K_console_formatter() << "mpv: Error applying low-latency profile";
        }
      }

      // load profiles list
      pfc::string8 profiles_str = get_string("profile-list");
      if (!profiles_str.is_empty()) {
        try {
          auto profiles_json = nlohmann::json::parse(profiles_str.c_str());
          for (auto it = profiles_json.rbegin(); it != profiles_json.rend();
               ++it) {
            std::string name = (*it)["name"];
            auto profilecond = (*it)["profile-cond"];
            // ignore built-in profiles; list might change in future
            if (profilecond.is_null() && name.compare("default") != 0 &&
                name.compare("gpu-hq") != 0 &&
                name.compare("low-latency") != 0 &&
                name.compare("pseudo-gui") != 0 &&
                name.compare("builtin-pseudo-gui") != 0 &&
                name.compare("libmpv") != 0 &&
                name.compare("encoding") != 0 &&
                name.compare("video") != 0 &&
                name.compare("albumart") != 0 &&
                name.compare("sw-fast") != 0 &&
                name.compare("opengl-hq") != 0) {
              std::lock_guard<std::mutex> lock(published_state_mutex);
              profiles.push_back(pfc::string8(name.c_str()));
            }
          }
        } catch (const std::exception& e) {
          if (cfg_logging) {
            FB2K_console_formatter()
                << "mpv: Could not read profile list: " << e.what();
          }
        }
      }

      const char* osc_cmd[] = {
          "script-message", "foobar", "osc-enabled-changed",
          cfg_osc && container->is_osc_enabled() ? "yes" : "no", nullptr};
      command(osc_cmd);
      set_property_string("fullscreen",
                          container->is_fullscreen() ? "yes" : "no");
    }
  }

  return mpv_handle != NULL;
}

void mpv_player::publish_titleformatting_subscriptions() {
  std::vector<titleformat_subscription> subscriptions;
  metadb_handle_ptr display_item;
  {
    std::lock_guard<std::mutex> lock(titleformat_mutex);
    subscriptions = titleformat_subscriptions;
  }
  {
    std::lock_guard<std::mutex> lock(display_item_mutex);
    display_item = current_display_item;
  }

  for (const auto& sub : subscriptions) {
    pfc::string8 format;
    if (display_item.is_valid()) {
      display_item->format_title(NULL, format, sub.object, NULL);
    }

    const char* cmd[] = {"script-message", "foobar",       "titleformat",
                         sub.id.c_str(),   format.c_str(), NULL};
    queue_command({cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]});
  }
}

void mpv_player::on_changed_sorted(metadb_handle_list_cref changed, bool) {
  metadb_handle_ptr display_item;
  {
    std::lock_guard<std::mutex> lock(display_item_mutex);
    display_item = current_display_item;
  }
  if (metadb_handle_list_helper::bsearch_by_pointer(
          changed, display_item) < UINT_MAX) {
    publish_titleformatting_subscriptions();

    if (mpv_state == state::Artwork) {
      reload_artwork();
    }
  }
}

void mpv_player::set_display_item(metadb_handle_ptr item) {
  {
    std::lock_guard<std::mutex> lock(display_item_mutex);
    current_display_item = item;
  }
  publish_titleformatting_subscriptions();
}

void mpv_player::on_selection_changed(metadb_handle_list_cref p_selection) {
  if (mpv_state == state::Idle || mpv_state == state::Artwork ||
      mpv_state == state::Unloaded) {
    request_artwork(p_selection);
  }
}

void mpv_player::on_volume_change(float new_vol) {
  std::string vol = std::to_string(VolumeMap::DBToSlider(new_vol));
  queue_command({"script-message", "foobar", "volume-changed", vol});
}

void mpv_player::on_playback_starting(play_control::t_track_command p_command,
                                      bool p_paused) {
  set_state(state::Preload);
  task t1;
  t1.type = task_type::Pause;
  t1.flag = p_paused;
  queue_task(t1);
}
void mpv_player::on_playback_new_track(metadb_handle_ptr p_track) {
  update_title();
  update();

  timing_info::refresh(false);

  task t;
  t.type = task_type::Play;
  t.play_file = p_track;
  t.time = 0.0;
  queue_task(t);
}
void mpv_player::on_playback_stop(play_control::t_stop_reason p_reason) {
  update_title();

  if (mpv_state != state::Artwork) {
    task t;
    t.type = task_type::Stop;
    queue_task(t);
  }
}
void mpv_player::on_playback_seek(double p_time) {
  update_title();

  timing_info::refresh(true);

  task t;
  t.type = task_type::Seek;
  t.time = p_time;
  t.flag = false;
  queue_task(t);
}
void mpv_player::on_playback_pause(bool p_state) {
  update_title();
  task t;
  t.type = task_type::Pause;
  t.flag = p_state;
  queue_task(t);
}
void mpv_player::on_playback_time(double) {
  update_title();
  update();
  task t;
  t.type = task_type::Sync;
  t.time = playback_control::get()->playback_get_position();
  t.sampled_at = std::chrono::steady_clock::now();
  t.flag = playback_control::get()->is_paused();
  queue_task(std::move(t));
}

void mpv_player::play(metadb_handle_ptr metadb, double time) {
  if (metadb.is_empty()) return;
  if (!mpv_handle && !mpv_init()) return;

  pfc::string8 filename;
  const bool is_local =
      filesystem::g_get_native_path(metadb->get_path(), filename);
  if (!is_local) {
    filename.reset();
    filename.add_filename(metadb->get_path());
  }

  apply_seek_offset = false;
  bool play_this = false;
  if (enabled) {
    if (is_local) {
      if (test_video_pattern(metadb)) {
        play_this = true;
      }
    } else if (cfg_foo_youtube && filename.has_prefix("\\fy+")) {
      if (cfg_remote_always_play || test_video_pattern(metadb)) {
        play_this = true;
        apply_seek_offset = true;
        filename.replace_string("\\fy+", "ytdl://");
      }
    } else if (cfg_ytdl_any) {
      if (cfg_remote_always_play || test_video_pattern(metadb)) {
        play_this = true;
        apply_seek_offset = true;
        filename.replace_string("\\fy+", "\\");
        filename.replace_string("\\", "ytdl://");
      }
    }
  }

  if (play_this) {
    if (time == 0.0) last_sync_time = 0;
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Playing URI " << filename;
    }

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

    double seek_offset = apply_seek_offset ? (double)cfg_remote_offset : 0.0;
    last_mpv_seek = ceil(1000 * (time + seek_offset + time_base)) / 1000.0;
    last_hard_sync = -99;

    std::stringstream time_sstring;
    time_sstring.setf(std::ios::fixed);
    time_sstring.precision(3);
    time_sstring << time_base + time + seek_offset;
    std::string time_string = time_sstring.str();
    set_option_string("start", time_string.c_str());
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Loading item '" << filename << "' at start time "
          << time_base + time + seek_offset;
    }

    const char* cmd_profile[] = {"apply-profile", "video", NULL};
    if (command(cmd_profile) < 0 && cfg_logging) {
      FB2K_console_formatter() << "mpv: Error loading video profile";
    }

    if (cfg_deint) {
      const char* cmd_deint[] = {"vf", "add", "@foompvdeint:bwdif:deint=1",
                                 NULL};
      if (command(cmd_deint) < 0 && cfg_logging) {
        FB2K_console_formatter() << "mpv: Error enabling deinterlacing";
      }
    }

    metadb_handle_ptr display_item;
    {
      std::lock_guard<std::mutex> lock(display_item_mutex);
      display_item = current_display_item;
    }
    bool next_chapter =
        mpv_state == state::Active && display_item.is_valid() && time == 0.0 &&
        display_item->get_path() && metadb->get_path() &&
        uStringCompare(display_item->get_path(), metadb->get_path()) == 0 &&
        display_item->get_subsong_index() + 1 == metadb->get_subsong_index();

    // reset speed
    double unity = 1.0;
    if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &unity) < 0 &&
        cfg_logging) {
      FB2K_console_formatter() << "mpv: Error setting speed";
    }

    if (next_chapter) {
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Playing next chapter";
      }
    } else {
      set_state(state::Loading);
      const char* cmd[] = {"loadfile", filename.c_str(), NULL};
      if (command(cmd) < 0 && cfg_logging) {
        FB2K_console_formatter()
            << "mpv: Error loading item '" << filename << "'";
      }
    }
    set_display_item(metadb);

    std::string start = std::to_string(time_base);
    const char* osc_cmd_1[] = {"script-message", "foobar", "start-changed",
                               start.c_str(), NULL};
    command(osc_cmd_1);

    std::string finish = std::to_string(time_base + metadb->get_length());
    const char* osc_cmd_2[] = {"script-message", "foobar", "finish-changed",
                               finish.c_str(), NULL};
    command(osc_cmd_2);

    std::weak_ptr<void> lifetime(lifetime_token);
    fb2k::inMainThread([lifetime]() {
      if (lifetime.expired() || !g_player) return;
      std::string vol = std::to_string(
          VolumeMap::DBToSlider(playback_control::get()->get_volume()));
      g_player->queue_command(
          {"script-message", "foobar", "volume-changed", vol});
    });

    if (!next_chapter) {
      // wait for file to load
      std::unique_lock<std::mutex> lock_starting(mutex);
      event_cv.wait(lock_starting, [this, filename]() {
        return check_queue_any() || mpv_state != state::Loading;
      });
      lock_starting.unlock();

      if (get_bool("pause")) {
        sync_on_unpause = true;
        if (cfg_logging) {
          FB2K_console_formatter() << "mpv: Setting sync_on_unpause after load";
        }
      } else {
        task t;
        t.type = task_type::FirstFrameSync;
        queue_task(t);
        if (cfg_logging) {
          FB2K_console_formatter()
              << "mpv: Starting first frame sync after load";
        }
      }
    }
  } else {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Not loading path " << metadb->get_path();
    }
    mpv_state = state::Idle;
    request_artwork();
    return;
  }
}

void mpv_player::stop() {
  if (!mpv_handle) return;

  sync_on_unpause = false;

  if (command_string("stop") < 0 && cfg_logging) {
    FB2K_console_formatter() << "mpv: Error stopping video";
  }
}

void mpv_player::pause(bool state) {
  if (!mpv_handle || !enabled || mpv_state == state::Idle) return;

  if (cfg_logging) {
    FB2K_console_formatter()
        << (state ? "mpv: Pause -> yes" : "mpv: Pause -> no");
  }

  if (set_property_string("pause", state ? "yes" : "no") < 0 && cfg_logging) {
    FB2K_console_formatter() << "mpv: Error pausing";
  }

  if (!state && sync_on_unpause) {
    sync_on_unpause = false;
    task t;
    t.type = task_type::FirstFrameSync;
    queue_task(t);
  }
}

void mpv_player::seek(double time, bool is_hard_sync) {
  if (!mpv_handle || !enabled || mpv_state == state::Idle) return;

  double seek_offset = apply_seek_offset ? (double)cfg_remote_offset : 0.0;
  if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Seeking to " << time << "+" << seek_offset
                             << "=" << time + seek_offset;
  }

  last_mpv_seek = time_base + time + seek_offset;
  last_hard_sync = is_hard_sync ? time + seek_offset : -99;
  if (!is_hard_sync) last_sync_time = static_cast<long>(std::floor(time));
  // reset speed
  double unity = 1.0;
  if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &unity) < 0 &&
      cfg_logging) {
    FB2K_console_formatter() << "mpv: Error setting speed";
  }

  // build command
  std::stringstream time_sstring;
  time_sstring.setf(std::ios::fixed);
  time_sstring.precision(15);
  time_sstring << time_base + time + seek_offset;
  std::string time_string = time_sstring.str();
  const char* cmd[] = {"seek", time_string.c_str(), "absolute+exact", NULL};

  if (command(cmd) < 0) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Cannot seek, waiting for file to load first";
    }

    while (true) {
      std::unique_lock<std::mutex> lock(mutex);
      event_cv.wait(lock, [this]() {
        return check_queue_any() || mpv_state == state::Idle ||
               mpv_state == state::Shutdown || mpv_state == state::Active ||
               mpv_state == state::Artwork;
      });
      lock.unlock();

      if (check_queue_time_change_locking() || mpv_state == state::Idle ||
          mpv_state == state::Shutdown || mpv_state == state::Artwork) {
        if (cfg_logging) {
          FB2K_console_formatter() << "mpv: Aborting seeking";
        }
        return;
      }

      if (mpv_state == state::Active) {
        if (command(cmd) < 0) {
          Sleep(20);
        } else {
          FB2K_console_formatter() << "mpv: Seeking started";
          break;
        }
      }
    }
  }

  if (get_bool("pause")) {
    sync_on_unpause = true;
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Queueing sync after paused seek";
    }
  } else {
    task t;
    t.type = task_type::FirstFrameSync;
    queue_task(t);
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Starting first frame sync after seek";
    }
  }
}

void mpv_player::sync(
    double fb_time, bool paused,
    std::chrono::steady_clock::time_point sampled_at) {
  last_sync_time = std::lround(fb_time);

  if (!get_bool("seekable") && mpv_state == state::Active) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Ignoring sync at " << fb_time << " - file not seekable";
    }
    return;
  }

  double mpv_time = -1.0;
  if (!mpv_handle || !enabled || mpv_state != state::Active || paused ||
      get_property("time-pos", libmpv::MPV_FORMAT_DOUBLE, &mpv_time) < 0) {
    return;
  }

  const double sample_age = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - sampled_at)
                                .count();
  if (sample_age > 1.0) return;
  fb_time += sample_age;

  double desync = time_base + fb_time - mpv_time;
  double new_speed = 1.0;

  if (running_ffs) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Skipping regular sync at " << fb_time;
    }
    return;
  }

  if (std::abs(desync) > 0.001 * cfg_hard_sync_threshold &&
      (fb_time - last_hard_sync) > cfg_hard_sync_interval) {
    // hard sync
    std::weak_ptr<void> lifetime(lifetime_token);
    fb2k::inMainThread([this, lifetime]() {
      if (lifetime.expired()) return;
      timing_info::refresh(false);
      task t;
      t.type = task_type::Seek;
      t.time = playback_control::get()->playback_get_position();
      t.flag = true;
      queue_task(t);
    });
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Hard a/v sync at " << fb_time << ", offset " << desync;
    }
  } else {
    // soft sync
    if (std::abs(desync) > 0.001 * cfg_max_drift) {
      // aim to correct mpv internal timer in 1 second, then let mpv catch up
      // the video
      new_speed = min(max(1.0 + desync, 0.01), 100.0);
    }

    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Sync at " << fb_time << " video offset " << desync
          << "; setting mpv speed to " << new_speed;
    }

    if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_logging) {
      FB2K_console_formatter() << "mpv: Error setting speed";
    }
  }
}

void mpv_player::on_new_artwork() {
  if (g_player && (g_player->mpv_state == state::Artwork ||
                   g_player->mpv_state == state::Idle ||
                   g_player->mpv_state == state::Unloaded)) {
    task t;
    t.type = task_type::LoadArtwork;
    g_player->queue_task(t);
  } else if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Ignoring loaded artwork";
  }
}

void mpv_player::load_artwork() {
  if (!mpv_handle && !mpv_init()) return;

  if (mpv_state == state::Idle || mpv_state == state::Artwork) {
    if (artwork_loaded()) {
      const char* cmd_profile[] = {"apply-profile", "albumart", NULL};
      if (command(cmd_profile) < 0 && cfg_logging) {
        FB2K_console_formatter() << "mpv: Error loading albumart profile";
      }

      const char* cmd_deint[] = {"vf", "remove", "@foompvdeint", NULL};
      command(cmd_deint);

      const char* cmd[] = {"loadfile", "artwork://", NULL};
      if (command(cmd) < 0) {
        FB2K_console_formatter() << "mpv: Error loading artwork";
      } else if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Loading artwork";
      }

      set_display_item(single_artwork_item());
    } else {
      task t;
      t.type = task_type::Stop;
      queue_task(t);

      set_display_item(NULL);
    }
  }
}

void mpv_player::initial_sync() {
  if (!mpv_handle || !enabled) return;

  const std::optional<timing_info::timing_info> timing = timing_info::get();
  if (!timing) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Abort initial sync - audio timing unavailable";
    }
    return;
  }

  {
    std::lock_guard<std::mutex> queuelock(mutex);
    if (check_queue_any()) {
      return;
    }
  }

  sync_on_unpause = false;

  int paused_check = 0;
  get_property("pause", libmpv::MPV_FORMAT_FLAG, &paused_check);
  if (paused_check == 1) {
    FB2K_console_formatter() << "mpv: Abort initial sync - player was paused "
                                "when starting initial "
                                "sync";
    return;
  }

  if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Initial sync";
  }

  if (libmpv::get()->observe_property(mpv_handle, time_pos_userdata, "time-pos",
                                      libmpv::MPV_FORMAT_DOUBLE) < 0) {
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Error observing time-pos";
    }
    return;
  }

  mpv_timepos = -1;
  while (true) {
    std::unique_lock<std::mutex> lock(mutex);
    event_cv.wait(lock, [this]() {
      return check_queue_any() || mpv_state == state::Idle ||
             mpv_state == state::Shutdown ||
             (mpv_state != state::Seeking && mpv_timepos > last_mpv_seek);
    });

    if (check_queue_any()) {
      libmpv::get()->unobserve_property(mpv_handle, time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - cmd";
      }
      lock.unlock();
      return;
    }

    if (mpv_state == state::Idle) {
      libmpv::get()->unobserve_property(mpv_handle, time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - idle";
      }
      lock.unlock();
      return;
    }

    if (mpv_state == state::Shutdown) {
      libmpv::get()->unobserve_property(mpv_handle, time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - shutdown";
      }
      lock.unlock();
      return;
    }

    if (mpv_state != state::Seeking && mpv_timepos > last_mpv_seek) {
      // frame decoded, wait for fb
      if (cfg_logging) {
        FB2K_console_formatter()
            << "mpv: First frame found at timestamp " << mpv_timepos
            << " after seek to " << last_mpv_seek << ", pausing";
      }

      if (set_property_string("pause", "yes") < 0 && cfg_logging) {
        FB2K_console_formatter() << "mpv: Error pausing";
      }

      lock.unlock();
      break;
    }

    lock.unlock();
  }

  libmpv::get()->unobserve_property(mpv_handle, time_pos_userdata);

  if (!get_bool("seekable") && mpv_state == state::Active) {
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Abort initial sync - file not seekable";
    }
    if (set_property_string("pause", "no") < 0 && cfg_logging) {
      FB2K_console_formatter() << "mpv: Error unpausing";
    }
    return;
  }

  // wait for fb to catch up to the first frame
  double vis_time = 0.0;
  visualisation_stream::ptr vis_stream = NULL;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  if (!vis_stream.is_valid()) {
    FB2K_console_formatter()
        << "mpv: Video disabled: this output has no timing "
           "information";
    if (set_property_string("pause", "no") < 0 && cfg_logging) {
      FB2K_console_formatter() << "mpv: Error pausing";
    }
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Abort initial sync - timing";
    }
    std::weak_ptr<void> lifetime(lifetime_token);
    fb2k::inMainThread([lifetime]() {
      if (lifetime.expired()) return;
      cfg_video_enabled = false;
      if (g_player) g_player->update();
    });
    return;
  }
  if (!vis_stream->get_absolute_time(vis_time)) {
    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Abort initial sync - audio timing unavailable";
    }
    if (set_property_string("pause", "no") < 0 && cfg_logging) {
      FB2K_console_formatter() << "mpv: Error unpausing";
    }
    return;
  }

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Audio time "
        << time_base + timing->last_fb_seek + vis_time -
               timing->last_seek_vistime;
  }

  while (time_base + timing->last_fb_seek + vis_time -
             timing->last_seek_vistime <
         mpv_timepos) {
    {
      std::lock_guard<std::mutex> queuelock(mutex);
      if (check_queue_any()) {
        if (set_property_string("pause", "no") < 0 && cfg_logging) {
          FB2K_console_formatter() << "mpv: Error pausing";
        }
        if (cfg_logging) {
          FB2K_console_formatter() << "mpv: Abort initial sync - command";
        }
        return;
      }
    }
    Sleep(10);
    if (!vis_stream->get_absolute_time(vis_time)) {
      FB2K_console_formatter()
          << "mpv: Initial sync failed, maybe this output does not "
             "have accurate "
             "timing info";
      if (set_property_string("pause", "no") < 0 && cfg_logging) {
        FB2K_console_formatter() << "mpv: Error pausing";
      }
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - timing";
      }
      return;
    }
  }

  double fb_time =
      timing->last_fb_seek + vis_time - timing->last_seek_vistime;

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Resuming, audio time " << time_base + fb_time;
  }
  if (set_property_string("pause", "no") < 0 && cfg_logging) {
    FB2K_console_formatter() << "mpv: Error pausing";
  }

  double desync = time_base + fb_time - mpv_timepos;
  // if mpv is behind on start, catch up
  if (desync > 0.001 * cfg_hard_sync_threshold &&
      (fb_time - last_hard_sync) > cfg_hard_sync_interval) {
    // hard sync
    task t;
    t.type = task_type::Seek;
    t.time = fb_time;
    t.flag = true;
    queue_task(t);
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Behind on initial sync - seeking";
    }
    return;
  } else if (desync > 0) {
    // maybe soft sync
    double time_before_next_sync = last_sync_time + 1.0 - fb_time;
    // prefer to wait if next sync is soon instead of setting speed extremely
    // high
    if (time_before_next_sync < 0.05) {
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Next sync " << last_sync_time + 1
                                 << " too close, returning ";
      }
      return;
    }
    double new_speed = 1.0;
    if (std::abs(desync) > 0.001 * cfg_max_drift) {
      // aim to correct mpv internal timer by next sync time
      new_speed = min(max(1.0 + desync / (time_before_next_sync), 0.01), 100.0);
    }

    if (cfg_logging) {
      FB2K_console_formatter()
          << "mpv: At initial sync, desync " << desync << "; catching up in "
          << time_before_next_sync << " before sync at " << last_sync_time + 1.0
          << "; setting mpv speed to " << new_speed;
    }

    if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_logging) {
      FB2K_console_formatter() << "mpv: Error setting speed";
    }
    // unset before we release the lock so the next sync isn't missed
    running_ffs = false;
    return;
  } else if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: At initial sync, desync " << desync << "; returning";
  }
}

int mpv_player::set_option_string(const char* name, const char* data) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_option_string(mpv_handle, name, data);
}

int mpv_player::set_property_string(const char* name, const char* data) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_property_string(mpv_handle, name, data);
}

int mpv_player::command_string(const char* args) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->command_string(mpv_handle, args);
}

int mpv_player::get_property(const char* name, libmpv::mpv_format format,
                             void* data) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->get_property(mpv_handle, name, format, data);
}

int mpv_player::command(const char** args) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->command(mpv_handle, args);
}

int mpv_player::set_option(const char* name, libmpv::mpv_format format,
                           void* data) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_option(mpv_handle, name, format, data);
}

pfc::string8 mpv_player::get_string(const char* name) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  pfc::string8 result;
  if (!mpv_handle) return result;

  char* value = libmpv::get()->get_property_string(mpv_handle, name);
  if (value != nullptr) {
    try {
      result = value;
    } catch (...) {
      libmpv::get()->free(value);
      throw;
    }
    libmpv::get()->free(value);
  }
  return result;
}

bool mpv_player::get_bool(const char* name) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return false;
  int flag = 0;
  libmpv::get()->get_property(mpv_handle, name, libmpv::MPV_FORMAT_FLAG, &flag);
  return flag == 1;
}

double mpv_player::get_double(const char* name) {
  std::lock_guard<std::recursive_mutex> lock(mpv_mutex);
  if (!mpv_handle) return 0;
  double num = 0;
  libmpv::get()->get_property(mpv_handle, name, libmpv::MPV_FORMAT_DOUBLE,
                              &num);
  return num;
}
}  // namespace mpv
