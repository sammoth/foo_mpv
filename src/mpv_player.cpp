#include "stdafx.h"
// PCH ^
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <set>
#include <thread>

#include "../helpers/VolumeMap.h"
#include "../helpers/atl-misc.h"
#include "../helpers/win32_misc.h"
#include "artwork_protocol.h"
#include "json.hpp"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"
#include "timing_info.h"

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

static const int time_pos_userdata = 28903278;
static const int seeking_userdata = 982628764;
static const int path_userdata = 982628764;
static const int idle_active_userdata = 12792384;

static metadb_handle_ptr current_selection;

extern cfg_bool cfg_video_enabled, cfg_black_fullscreen, cfg_stop_hidden,
    cfg_artwork, cfg_osc;
extern cfg_uint cfg_bg_color, cfg_artwork_type;
extern advconfig_checkbox_factory cfg_logging, cfg_mpv_logfile;
extern advconfig_integer_factory cfg_max_drift, cfg_hard_sync_threshold,
    cfg_hard_sync_interval, cfg_seek_seconds;

mpv_player::mpv_player()
    : enabled(false),
      mouse_over(false),
      mpv_handle(nullptr, nullptr),
      mpv_window_hwnd(NULL),
      task_queue(),
      sync_on_unpause(false),
      last_mpv_seek(0),
      last_hard_sync(-99),
      last_sync_time(0),
      running_ffs(false),
      mpv_timepos(0),
      mpv_state(state::Unloaded),
      container(mpv_container::get_main_container()),
      time_base(0) {
  control_thread = std::thread([this]() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex);
      control_thread_cv.wait(lock, [this] { return !task_queue.empty(); });
      task task = task_queue.front();
      task_queue.pop();
      lock.unlock();

      switch (task.type) {
        case task_type::Quit:
          return;
        case task_type::Play:
          play(task.play_file, task.time);
          break;
        case task_type::Stop:
          stop();
          break;
        case task_type::Seek:
          seek(task.time);
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
      }
    }
  });
}

mpv_player::~mpv_player() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    while (!task_queue.empty()) task_queue.pop();
    task t;
    t.type = task_type::Quit;
    task_queue.push(t);
  }
  control_thread_cv.notify_all();

  if (control_thread.joinable()) control_thread.join();
  if (event_listener.joinable()) event_listener.join();
}

bool mpv_player::check_queue_any() { return !task_queue.empty(); }

void mpv_player::queue_task(task t) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    task_queue.push(t);
  }
  control_thread_cv.notify_all();
  event_cv.notify_all();
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
  WIN32_OP_D(brush.CreateSolidBrush(cfg_bg_color) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));
  return TRUE;
}

enum {
  ID_ENABLED = 1,
  ID_FULLSCREEN = 2,
  ID_ART_FRONT = 3,
  ID_ART_BACK = 4,
  ID_ART_DISC = 5,
  ID_ART_ARTIST = 6,
  ID_STATS = 99,
  ID_PROFILES = 5000,
};

void mpv_player::add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
  if (g_player && g_player->mpv_handle) {
    if (g_player->mpv_state == state::Idle ||
        g_player->mpv_state == state::Artwork) {
      menu->AppendMenu(MF_DISABLED, ID_STATS, _T("Idle"));
    } else {
      std::wstringstream text;
      text.setf(std::ios::fixed);
      text.precision(3);
      text << g_player->get_string("video-codec") << " "
           << g_player->get_string("video-params/pixelformat");
      menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      text.str(L"");
      text << g_player->get_string("width") << "x"
           << g_player->get_string("height") << " "
           << g_player->get_double("container-fps") << "fps (display "
           << g_player->get_double("estimated-vf-fps") << "fps)";
      menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      std::string hwdec = g_player->get_string("hwdec-current");
      if (hwdec != "no") {
        text.str(L"");
        text << "Hardware decoding: " << g_player->get_string("hwdec-current");
        menu->AppendMenu(MF_DISABLED, ID_STATS, text.str().c_str());
      }

      menu->AppendMenu(MF_SEPARATOR, ID_STATS, _T(""));

      if (g_player->profiles.size() > 0) {
        for (int i = 0; i < g_player->profiles.size(); i++) {
          uAppendMenu(menu->m_hMenu, MF_DEFAULT, ID_PROFILES + i,
                      g_player->profiles[i].c_str());
        }
        menu->AppendMenu(MF_SEPARATOR, ID_STATS, _T(""));
      }
    }
  }

  menu->AppendMenu(cfg_video_enabled ? MF_CHECKED : MF_UNCHECKED, ID_ENABLED,
                   _T("Enabled"));
  menudesc->Set(ID_ENABLED, "Enable/disable video playback");
  menu->AppendMenu(
      g_player->container->is_fullscreen() ? MF_CHECKED : MF_UNCHECKED,
      ID_FULLSCREEN, _T("Fullscreen"));
  menudesc->Set(ID_FULLSCREEN, "Toggle video fullscreen");

  if (cfg_artwork &&
      (!g_player->mpv_handle || g_player->mpv_state == state::Idle ||
       g_player->mpv_state == state::Artwork)) {
    menu->AppendMenu(MF_SEPARATOR, ID_STATS, _T(""));
    menu->AppendMenu(cfg_artwork_type == 0 ? MF_CHECKED : MF_UNCHECKED,
                     ID_ART_FRONT, _T("Front"));
    menu->AppendMenu(cfg_artwork_type == 1 ? MF_CHECKED : MF_UNCHECKED,
                     ID_ART_BACK, _T("Back"));
    menu->AppendMenu(cfg_artwork_type == 2 ? MF_CHECKED : MF_UNCHECKED,
                     ID_ART_DISC, _T("Disc"));
    menu->AppendMenu(cfg_artwork_type == 3 ? MF_CHECKED : MF_UNCHECKED,
                     ID_ART_ARTIST, _T("Artist"));
  }
}

void mpv_player::handle_menu_cmd(int cmd) {
  if (g_player) {
    switch (cmd) {
      case ID_ENABLED:
        cfg_video_enabled = !cfg_video_enabled;
        g_player->update();
        break;
      case ID_FULLSCREEN:
        g_player->container->toggle_fullscreen();
        break;
      case ID_ART_FRONT:
        cfg_artwork_type = 0;
        request_artwork(current_selection);
        break;
      case ID_ART_BACK:
        cfg_artwork_type = 1;
        request_artwork(current_selection);
        break;
      case ID_ART_DISC:
        cfg_artwork_type = 2;
        request_artwork(current_selection);
        break;
      case ID_ART_ARTIST:
        cfg_artwork_type = 3;
        request_artwork(current_selection);
        break;
      default:
        int profile_id = cmd - ID_PROFILES;
        if (profile_id > -1 && profile_id < g_player->profiles.size()) {
          const char* cmd_profile[] = {
              "apply-profile", g_player->profiles[profile_id].c_str(), NULL};
          if (g_player->command(cmd_profile) < 0 && cfg_logging) {
            FB2K_console_formatter() << "mpv: Error loading video profile";
          }
        }
        break;
    }
  }
}

void mpv_player::on_context_menu(CWindow wnd, CPoint point) {
  container->on_context_menu(wnd, point);
}

void mpv_player::on_double_click(UINT, CPoint) {
  container->toggle_fullscreen();
}

void mpv_player::on_destroy() {
  command_string("quit");
  g_player = NULL;
}

LRESULT mpv_player::on_create(LPCREATESTRUCT lpcreate) {
  SetClassLong(
      m_hWnd, GCL_HICON,
      (LONG)LoadIcon(core_api::get_my_instance(), MAKEINTRESOURCE(IDI_ICON1)));
  update();
  return 0;
}

void mpv_player::on_mouse_leave() {
  mouse_over = false;

  if (!mpv_handle) return;
  struct libmpv::mpv_node cmd[3] = {};

  const char* cmdname = "mouse";
  cmd[0].format = libmpv::MPV_FORMAT_STRING;
  cmd[1].format = libmpv::MPV_FORMAT_INT64;
  cmd[2].format = libmpv::MPV_FORMAT_INT64;
  cmd[0].u.string = (char*)cmdname;
  cmd[1].u.int64 = 1;
  cmd[2].u.int64 = 1;
  libmpv::mpv_node_list list;
  list.num = 3;
  list.values = cmd;

  libmpv::mpv_node args;
  args.format = libmpv::MPV_FORMAT_NODE_ARRAY;
  args.u.list = &list;
  libmpv::get()->command_node(mpv_handle.get(), &args, NULL);
}

void mpv_player::on_mouse_move(UINT, CPoint point) {
  if (!mpv_handle) return;

  if (!mouse_over) {
    mouse_over = true;
    TRACKMOUSEEVENT tme = {sizeof(tme)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    TrackMouseEvent(&tme);
  }

  struct libmpv::mpv_node cmd[3] = {};

  const char* cmdname = "mouse";
  cmd[0].format = libmpv::MPV_FORMAT_STRING;
  cmd[1].format = libmpv::MPV_FORMAT_INT64;
  cmd[2].format = libmpv::MPV_FORMAT_INT64;
  cmd[0].u.string = (char*)cmdname;
  cmd[1].u.int64 = point.x;
  cmd[2].u.int64 = point.y;
  libmpv::mpv_node_list list;
  list.num = 3;
  list.values = cmd;

  libmpv::mpv_node args;
  args.format = libmpv::MPV_FORMAT_NODE_ARRAY;
  args.u.list = &list;
  libmpv::get()->command_node(mpv_handle.get(), &args, NULL);
}

LRESULT mpv_player::on_mouse_wheel(UINT l, UINT h, CPoint point) {
  if (!mpv_handle) return 0;

  auto zDelta = GET_WHEEL_DELTA_WPARAM(MAKELPARAM(l, h));

  POINT point_struct = {point.x, point.y};

  ::MapWindowPoints(NULL, m_hWnd, &point_struct, 1);

  struct libmpv::mpv_node cmd[4] = {};

  const char* cmdname = "mouse";
  cmd[0].format = libmpv::MPV_FORMAT_STRING;
  cmd[1].format = libmpv::MPV_FORMAT_INT64;
  cmd[2].format = libmpv::MPV_FORMAT_INT64;
  cmd[3].format = libmpv::MPV_FORMAT_STRING;
  cmd[0].u.string = (char*)cmdname;
  cmd[1].u.int64 = point_struct.x;
  cmd[2].u.int64 = point_struct.y;
  cmd[3].u.string = zDelta >= 0 ? (char*)"3" : (char*)"4";
  libmpv::mpv_node_list list;
  list.num = 4;
  list.values = cmd;

  libmpv::mpv_node args;
  args.format = libmpv::MPV_FORMAT_NODE_ARRAY;
  args.u.list = &list;
  libmpv::get()->command_node(mpv_handle.get(), &args, NULL);

  return 0;
}

void mpv_player::on_mouse_down(UINT wp, CPoint point) {
  if (!mpv_window_hwnd) {
    mpv_window_hwnd = FindWindowEx(m_hWnd, NULL, L"mpv", NULL);
    if (!mpv_window_hwnd) return;
  }

  ::SendMessage(mpv_window_hwnd, WM_LBUTTONDOWN, wp,
                MAKELPARAM(point.x, point.y));
}

void mpv_player::on_mouse_up(UINT wp, CPoint point) {
  if (!mpv_window_hwnd) {
    mpv_window_hwnd = FindWindowEx(m_hWnd, NULL, L"mpv", NULL);
    if (!mpv_window_hwnd) return;
  }

  ::SendMessage(mpv_window_hwnd, WM_LBUTTONUP, wp,
                MAKELPARAM(point.x, point.y));
}

void mpv_player::update() {
  mpv_container* new_container = mpv_container::get_main_container();
  if (new_container == NULL) {
    mpv_container* old_container = container;
    container = new_container;
    if (old_container != NULL) {
      old_container->on_lose_player();
    }
    DestroyWindow();
    return;
  }

  if (container != new_container) {
    mpv_container* old_container = container;
    container = new_container;
    old_container->on_lose_player();
    new_container->on_gain_player();
  }

  const char* osc_cmd_1[] = {"script-message", "osc-setenabled",
                             cfg_osc && container->is_osc_enabled() ? "1" : "0",
                             NULL};
  command(osc_cmd_1);

  ResizeClient(container->cx,
               container->cy);  // wine is less buggy if we resize first

  if (GetParent() != container->container_wnd()) {
    SetParent(container->container_wnd());
    mpv_container::invalidate_all_containers();
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
  mpv::get_popup_title(title);

  const char* osc_cmd_1[] = {"script-message", "osc-settitle", title.c_str(),
                             NULL};
  command(osc_cmd_1);
  uSetWindowText(m_hWnd, title);
}

void mpv_player::set_background() {
  if (!mpv_handle) return;

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

bool mpv_player::mpv_init() {
  if (!libmpv::get()->ready) return false;
  std::lock_guard<std::mutex> lock_init(init_mutex);

  if (!mpv_handle && m_hWnd != NULL) {
    pfc::string_formatter path;
    path.add_filename(core_api::get_profile_path());
    path.add_filename("mpv");
    path.replace_string("\\file://", "");
    mpv_handle = {libmpv::get()->create(), libmpv::get()->terminate_destroy};

    int64_t l_wid = (intptr_t)(m_hWnd);
    set_option("wid", libmpv::MPV_FORMAT_INT64, &l_wid);

    set_option_string("config", "yes");
    set_option_string("config-dir", path.c_str());

    set_option_string("ytdl", "no");
    set_option_string("osc", "no");
    set_option_string("load-stats-overlay", "no");
    set_option_string("load-osd-console", "no");
    set_option_string("alpha", "blend");

    set_background();

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
    set_option_string("no-initial-audio-sync", "yes");

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

    pfc::string_formatter osc_path = core_api::get_my_full_path();
    osc_path.truncate(osc_path.scan_filename());
    osc_path << "mpv\\osc.lua";
    osc_path.replace_char('\\', '/', 0);
    set_option_string("scripts", osc_path.c_str());

    libmpv::get()->stream_cb_add_ro(mpv_handle.get(), "artwork", this,
                                    artwork_protocol_open);
    set_option_string("image-display-duration", "inf");

    libmpv::get()->observe_property(mpv_handle.get(), seeking_userdata,
                                    "seeking", libmpv::MPV_FORMAT_FLAG);
    libmpv::get()->observe_property(mpv_handle.get(), idle_active_userdata,
                                    "idle-active", libmpv::MPV_FORMAT_FLAG);
    libmpv::get()->observe_property(mpv_handle.get(), path_userdata, "path",
                                    libmpv::MPV_FORMAT_STRING);

    if (libmpv::get()->initialize(mpv_handle.get()) != 0) {
      libmpv::get()->destroy(mpv_handle.get());
      mpv_handle = NULL;
    } else {
      event_listener = std::thread([this]() {
        if (!mpv_handle) {
          FB2K_console_formatter()
              << "mpv: libmpv event listener started but mpv wasn't running";
          return;
        }

        while (true) {
          libmpv::mpv_event* event =
              libmpv::get()->wait_event(mpv_handle.get(), -1);

          {
            std::lock_guard<std::mutex> lock(mutex);

            if (event->event_id == libmpv::MPV_EVENT_CLIENT_MESSAGE) {
              libmpv::mpv_event_client_message* event_message =
                  (libmpv::mpv_event_client_message*)event->data;
              if (event_message->num_args > 0) {
                if (strcmp(event_message->args[0], "foobar-seek") == 0) {
                  if (event_message->num_args > 1) {
                    const char* time_str = event_message->args[1];
                    double time = std::stod(time_str);
                    fb2k::inMainThread([time]() {
                      playback_control::get()->playback_seek(time);
                    });
                  }
                } else if (strcmp(event_message->args[0], "foobar-pause") ==
                           0) {
                  fb2k::inMainThread(
                      []() { playback_control::get()->toggle_pause(); });
                } else if (strcmp(event_message->args[0], "foobar-prev") == 0) {
                  fb2k::inMainThread(
                      []() { playback_control::get()->previous(); });
                } else if (strcmp(event_message->args[0], "foobar-next") == 0) {
                  fb2k::inMainThread([]() { playback_control::get()->next(); });
                } else if (strcmp(event_message->args[0], "foobar-volup") ==
                           0) {
                  fb2k::inMainThread(
                      []() { playback_control::get()->volume_up(); });
                } else if (strcmp(event_message->args[0], "foobar-voldown") ==
                           0) {
                  fb2k::inMainThread(
                      []() { playback_control::get()->volume_down(); });
                } else if (strcmp(event_message->args[0],
                                  "foobar-fullscreen") == 0) {
                  fb2k::inMainThread([this]() {
                    if (container) container->toggle_fullscreen();
                  });
                } else if (strcmp(event_message->args[0], "foobar-seekback") ==
                               0 &&
                           cfg_seek_seconds > 0) {
                  fb2k::inMainThread([]() {
                    playback_control::get()->playback_seek_delta(
                        0.0 - cfg_seek_seconds);
                  });
                } else if (strcmp(event_message->args[0],
                                  "foobar-seekforward") == 0 &&
                           cfg_seek_seconds > 0) {
                  fb2k::inMainThread([]() {
                    playback_control::get()->playback_seek_delta(
                        cfg_seek_seconds);
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
                  event_property->format == libmpv::MPV_FORMAT_DOUBLE) {
                mpv_timepos = *(double*)(event_property->data);
              } else if (event->reply_userdata == seeking_userdata ||
                         event->reply_userdata == idle_active_userdata ||
                         event->reply_userdata == path_userdata) {
                bool idle = get_bool("idle-active");
                const char* path = get_string("path");
                bool showing_art =
                    path != NULL && strcmp(path, "artwork://") == 0;
                bool seeking = get_bool("seeking");

                state new_state =
                    showing_art ? state::Artwork
                                : seeking ? state::Seeking
                                          : idle ? state::Idle : state::Active;

                if (mpv_state == state::Preload && new_state == state::Idle) {
                  new_state = state::Preload;
                }

                if (mpv_state != new_state) {
                  if (new_state == state::Idle && mpv_state != state::Artwork) {
                    request_artwork(current_selection);
                  }

                  set_state(new_state);
                }
              }
            }
          }

          control_thread_cv.notify_all();
          event_cv.notify_all();
        }
      });

      char* profiles_str =
          libmpv::get()->get_property_string(mpv_handle.get(), "profile-list");

      auto profiles_json = nlohmann::json::parse(profiles_str);
      for (auto it = profiles_json.rbegin(); it != profiles_json.rend(); ++it) {
        std::string name = (*it)["name"];
        auto profilecond = (*it)["profile-cond"];
        // ignore built-in profiles; list might change in future
        if (profilecond.is_null() && name.compare("default") != 0 &&
            name.compare("gpu-hq") != 0 && name.compare("low-latency") != 0 &&
            name.compare("pseudo-gui") != 0 &&
            name.compare("builtin-pseudo-gui") != 0 &&
            name.compare("libmpv") != 0 && name.compare("encoding") != 0 &&
            name.compare("video") != 0 && name.compare("albumart") != 0 &&
            name.compare("sw-fast") != 0 && name.compare("opengl-hq") != 0) {
          profiles.push_back(name);
        }
      }
    }
  }

  return mpv_handle != NULL;
}

void mpv_player::on_selection_changed(metadb_handle_list_cref p_selection) {
  metadb_handle_ptr new_item;
  if (p_selection.get_count() > 0) new_item = p_selection[0];

  if (new_item == current_selection) return;

  current_selection = new_item;

  if (mpv_state == state::Idle || mpv_state == state::Artwork ||
      mpv_state == state::Unloaded) {
    if (current_selection.is_empty()) {
      task t;
      t.type = task_type::Stop;
      queue_task(t);
    } else {
      request_artwork(current_selection);
    }
  }
}

void mpv_player::on_volume_change(float new_vol) {
  std::string vol = std::to_string(VolumeMap::DBToSlider(new_vol));
  const char* cmd[] = {"script-message", "osc-setvolume", vol.c_str(), NULL};
  command(cmd);
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

  {
    std::lock_guard<std::mutex> lock(sync_lock);
    last_sync_time = 0;
  }

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

  {
    std::lock_guard<std::mutex> lock(sync_lock);
    last_sync_time = (int)floor(p_time);
  }

  task t;
  t.type = task_type::Seek;
  t.time = p_time;
  queue_task(t);
}
void mpv_player::on_playback_pause(bool p_state) {
  update_title();
  task t;
  t.type = task_type::Pause;
  t.flag = p_state;
  queue_task(t);
}
void mpv_player::on_playback_time(double p_time) {
  update_title();
  update();
  sync(p_time);
}

void mpv_player::play(metadb_handle_ptr metadb, double time) {
  if (metadb.is_empty()) return;
  if (!mpv_handle && !mpv_init()) return;

  if (!enabled) {
    mpv_state = state::Idle;
    request_artwork(current_selection);
    return;
  }

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

    last_mpv_seek = ceil(1000 * (time + time_base)) / 1000.0;
    last_hard_sync = -99;

    std::stringstream time_sstring;
    time_sstring.setf(std::ios::fixed);
    time_sstring.precision(3);
    time_sstring << time_base + time;
    std::string time_string = time_sstring.str();
    set_option_string("start", time_string.c_str());
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Loading item '" << filename
                               << "' at start time " << time_base + time;
    }

    // reset speed
    double unity = 1.0;
    if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &unity) < 0 &&
        cfg_logging) {
      FB2K_console_formatter() << "mpv: Error setting speed";
    }

    const char* cmd_profile[] = {"apply-profile", "video", NULL};
    if (command(cmd_profile) < 0 && cfg_logging) {
      FB2K_console_formatter() << "mpv: Error loading video profile";
    }
    set_state(state::Loading);
    const char* cmd[] = {"loadfile", filename.c_str(), NULL};
    if (command(cmd) < 0 && cfg_logging) {
      FB2K_console_formatter()
          << "mpv: Error loading item '" << filename << "'";
    }

    std::string start = std::to_string(time_base);
    const char* osc_cmd_1[] = {"script-message", "osc-setstart", start.c_str(),
                               NULL};
    command(osc_cmd_1);

    std::string finish = std::to_string(time_base + metadb->get_length());
    const char* osc_cmd_2[] = {"script-message", "osc-setfinish",
                               finish.c_str(), NULL};
    command(osc_cmd_2);

    fb2k::inMainThread([this]() {
      std::string vol = std::to_string(
          VolumeMap::DBToSlider(playback_control::get()->get_volume()));
      const char* osc_cmd_3[] = {"script-message", "osc-setvolume", vol.c_str(),
                                 NULL};
      command(osc_cmd_3);
    });

    // wait for file to load
    std::unique_lock<std::mutex> lock_starting(mutex);
    event_cv.wait(lock_starting, [this, filename]() {
      const char* path = get_string("path");
      return path != NULL && filename.equals(path);
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
        FB2K_console_formatter() << "mpv: Starting first frame sync after load";
      }
    }
  } else if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Skipping loading item '" << filename
                             << "' because it is not a local file";
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

void mpv_player::seek(double time) {
  if (!mpv_handle || !enabled || mpv_state == state::Idle) return;

  if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Seeking to " << time;
  }

  last_mpv_seek = time_base + time;
  last_hard_sync = -99;
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
  time_sstring << time_base + time;
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
        return mpv_state == state::Idle || mpv_state == state::Shutdown ||
               mpv_state == state::Active || mpv_state == state::Artwork;
      });
      lock.unlock();

      if (mpv_state == state::Idle || mpv_state == state::Shutdown ||
          mpv_state == state::Artwork) {
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

void mpv_player::sync(double debug_time) {
  std::lock_guard<std::mutex> lock(sync_lock);
  last_sync_time = std::lround(debug_time);

  double mpv_time = -1.0;
  if (!mpv_handle || !enabled || mpv_state != state::Active ||
      playback_control::get()->is_paused() ||
      get_property("time-pos", libmpv::MPV_FORMAT_DOUBLE, &mpv_time) < 0) {
    return;
  }

  double fb_time = playback_control::get()->playback_get_position();
  double desync = time_base + fb_time - mpv_time;
  double new_speed = 1.0;

  if (abs(desync) > 0.001 * cfg_hard_sync_threshold &&
      (fb_time - last_hard_sync) > cfg_hard_sync_interval) {
    // hard sync
    timing_info::refresh(false);
    {
      task t;
      t.type = task_type::Seek;
      t.time = fb_time;
      queue_task(t);
    }
    last_hard_sync = fb_time;
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Hard a/v sync";
    }
  } else {
    if (running_ffs) {
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Skipping regular sync";
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
      FB2K_console_formatter()
          << "mpv: Sync at " << debug_time << " video offset " << desync
          << "; setting mpv speed to " << new_speed;
    }

    if (set_option("speed", libmpv::MPV_FORMAT_DOUBLE, &new_speed) < 0 &&
        cfg_logging) {
      FB2K_console_formatter() << "mpv: Error setting speed";
    }
  }
}

void mpv_player::on_new_artwork() {
  if (g_player) {
    task t;
    t.type = task_type::LoadArtwork;
    g_player->queue_task(t);
  }
}

void mpv_player::load_artwork() {
  if (!mpv_handle && !mpv_init()) return;

  if (mpv_state == state::Idle || mpv_state == state::Artwork ||
      mpv_state == state::Preload) {
    if (artwork_loaded()) {
      const char* cmd_profile[] = {"apply-profile", "albumart", NULL};
      if (command(cmd_profile) < 0 && cfg_logging) {
        FB2K_console_formatter() << "mpv: Error loading albumart profile";
      }
      const char* cmd[] = {"loadfile", "artwork://", NULL};
      if (command(cmd) < 0) {
        FB2K_console_formatter() << "mpv: Error loading artwork";
      } else if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Loading artwork";
      }
    } else {
      task t;
      t.type = task_type::Stop;
      queue_task(t);
    }
  }
}

void mpv_player::initial_sync() {
  if (!mpv_handle || !enabled) return;

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
    FB2K_console_formatter()
        << "mpv: Abort initial sync - player was paused when starting initial "
           "sync";
    return;
  }

  if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Initial sync";
  }

  if (libmpv::get()->observe_property(mpv_handle.get(), time_pos_userdata,
                                      "time-pos",
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
             mpv_state == state::Shutdown || mpv_timepos > last_mpv_seek;
    });

    if (check_queue_any()) {
      libmpv::get()->unobserve_property(mpv_handle.get(), time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - cmd";
      }
      lock.unlock();
      return;
    }

    if (mpv_state == state::Idle) {
      libmpv::get()->unobserve_property(mpv_handle.get(), time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - idle";
      }
      lock.unlock();
      return;
    }

    if (mpv_state == state::Shutdown) {
      libmpv::get()->unobserve_property(mpv_handle.get(), time_pos_userdata);
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Abort initial sync - shutdown";
      }
      lock.unlock();
      return;
    }

    if (mpv_timepos > last_mpv_seek) {
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

  libmpv::get()->unobserve_property(mpv_handle.get(), time_pos_userdata);

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
    fb2k::inMainThread([this]() {
      cfg_video_enabled = false;
      update();
    });
    return;
  }
  vis_stream->get_absolute_time(vis_time);

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Audio time "
        << time_base + timing_info::get().last_fb_seek + vis_time -
               timing_info::get().last_seek_vistime;
  }

  int count = 0;
  while (time_base + timing_info::get().last_fb_seek + vis_time -
             timing_info::get().last_seek_vistime <
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
    vis_stream->get_absolute_time(vis_time);
    if (count++ > 1000 && !vis_stream->get_absolute_time(vis_time)) {
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

  double fb_time = timing_info::get().last_fb_seek + vis_time -
                   timing_info::get().last_seek_vistime;

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Resuming, audio time " << time_base + fb_time;
  }
  if (set_property_string("pause", "no") < 0 && cfg_logging) {
    FB2K_console_formatter() << "mpv: Error pausing";
  }

  double desync = time_base + fb_time - mpv_timepos;
  // if mpv is behind on start, catch up
  if (desync > cfg_hard_sync_threshold) {
    // hard sync
    task t;
    t.type = task_type::Seek;
    t.time = time_base + fb_time;
    queue_task(t);
    last_hard_sync = fb_time;
    if (cfg_logging) {
      FB2K_console_formatter() << "mpv: Behind on initial sync - seeking";
    }
    return;
  } else if (desync > 0) {
    // maybe soft sync
    std::lock_guard<std::mutex> lock(sync_lock);

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
    if (abs(desync) > 0.001 * cfg_max_drift) {
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
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_option_string(mpv_handle.get(), name, data);
}

int mpv_player::set_property_string(const char* name, const char* data) {
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_property_string(mpv_handle.get(), name, data);
}

int mpv_player::command_string(const char* args) {
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->command_string(mpv_handle.get(), args);
}

int mpv_player::get_property(const char* name, libmpv::mpv_format format,
                             void* data) {
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->get_property(mpv_handle.get(), name, format, data);
}

int mpv_player::command(const char** args) {
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->command(mpv_handle.get(), args);
}

int mpv_player::set_option(const char* name, libmpv::mpv_format format,
                           void* data) {
  if (!mpv_handle) return libmpv::MPV_ERROR_UNINITIALIZED;
  return libmpv::get()->set_option(mpv_handle.get(), name, format, data);
}

const char* mpv_player::get_string(const char* name) {
  if (!mpv_handle) return "";
  const char* ret = libmpv::get()->get_property_string(mpv_handle.get(), name);
  if (ret == NULL) return "";
  return ret;
}

bool mpv_player::get_bool(const char* name) {
  if (!mpv_handle) return false;
  int flag = 0;
  libmpv::get()->get_property(mpv_handle.get(), name, libmpv::MPV_FORMAT_FLAG,
                              &flag);
  return flag == 1;
}

double mpv_player::get_double(const char* name) {
  if (!mpv_handle) return 0;
  double num = 0;
  libmpv::get()->get_property(mpv_handle.get(), name, libmpv::MPV_FORMAT_DOUBLE,
                              &num);
  return num;
}
}  // namespace mpv