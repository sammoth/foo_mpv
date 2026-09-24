#pragma once
#include "stdafx.h"
// PCH ^
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "libmpv.h"
#include "mpv_container.h"

namespace mpv {

void get_popup_title(pfc::string8& s);

class mpv_player : play_callback_impl_base,
                   ui_selection_callback_impl_base,
                   metadb_io_callback_dynamic_impl_base,
                   public CWindowImpl<mpv_player> {
  // The control thread owns the libmpv handle and all mutating libmpv calls.
  // The event thread only blocks in mpv_wait_event and publishes event data.
  libmpv::mpv_handle* mpv_handle;
  std::recursive_mutex mpv_mutex;
  std::atomic_bool mpv_loaded = false;
  HWND mpv_window_hwnd;
  std::atomic_bool enabled;
  std::shared_ptr<void> lifetime_token = std::make_shared<int>(0);

  // start mpv within the window
  bool mpv_init();
  std::mutex init_mutex;

  // thread for dispatching libmpv events
  std::thread event_listener;
  std::atomic_bool event_listener_stop = false;
  std::condition_variable event_cv;
  std::mutex mutex;
  std::atomic<double> mpv_timepos;
  enum class state {
    Unloaded,
    Preload,
    Loading,
    Active,
    Idle,
    Seeking,
    Artwork,
    Shutdown
  };
  std::atomic<state> mpv_state;
  void set_state(state new_state);

  // thread for deferred player control tasks
  enum class task_type {
    Quit,
    FirstFrameSync,
    Play,
    Seek,
    Pause,
    Stop,
    LoadArtwork,
    Command,
    Sync,
    HideCursorForMenu,
    RestoreCursorAfterMenu,
    RefreshMediaInfo
  };
  struct task {
    task_type type = task_type::Stop;
    metadb_handle_ptr play_file;
    double time = 0.0;
    bool flag = false;
    std::chrono::steady_clock::time_point sampled_at;
    std::vector<std::string> arguments;
  };
  std::thread control_thread;
  std::condition_variable control_thread_cv;
  std::atomic_bool running_ffs;
  std::deque<task> task_queue;
  void queue_task(task t);
  void queue_command(std::initializer_list<std::string> arguments);
  void run_command(const std::vector<std::string>& arguments);
  void refresh_media_info();
  bool check_queue_any();
  bool check_queue_time_change_locking();

  // methods run off thread
  void play(metadb_handle_ptr metadb, double start_time);
  void stop();
  void pause(bool p_state);
  void seek(double time, bool is_hard_sync);
  void sync(double fb_time, bool paused,
            std::chrono::steady_clock::time_point sampled_at);
  void initial_sync();
  void load_artwork();

  // Control-thread state. Atomics are read-only snapshots for the UI/event
  // paths; writes happen on the control thread unless driven by an mpv event.
  std::atomic<double>
      time_base;  // start time of the current track/subsong within its file
  std::atomic<double> last_mpv_seek;
  std::atomic_bool sync_on_unpause;
  double last_hard_sync;
  long last_sync_time;
  bool apply_seek_offset = false;
  pfc::string8 cursor_autohide_before_menu = "1000";

  std::vector<pfc::string8> profiles;
  std::mutex published_state_mutex;
  pfc::string8 media_codec_info;
  pfc::string8 media_display_info;
  pfc::string8 media_hwdec_info;

  // utils
  pfc::string8 get_string(const char* name);
  bool get_bool(const char* name);
  double get_double(const char* name);
  int command_string(const char* args);
  int set_option_string(const char* name, const char* data);
  int set_property_string(const char* name, const char* data);
  int get_property(const char* name, libmpv::mpv_format format, void* data);
  int command(const char** args);
  int set_option(const char* name, libmpv::mpv_format format, void* data);

  // play callbacks
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused);
  void on_playback_new_track(metadb_handle_ptr p_track);
  void on_playback_stop(play_control::t_stop_reason p_reason);
  void on_playback_seek(double p_time);
  void on_playback_pause(bool p_state);
  void on_playback_time(double p_time);
  void on_volume_change(float new_vol);

  // scripting/artwork
  void on_selection_changed(metadb_handle_list_cref) override;
  void on_changed_sorted(metadb_handle_list_cref, bool) override;
  metadb_handle_ptr current_display_item;
  std::mutex display_item_mutex;
  void set_display_item(metadb_handle_ptr item);
  struct titleformat_subscription {
    pfc::string8 id;
    titleformat_object::ptr object;
  };
  std::vector<titleformat_subscription> titleformat_subscriptions;
  std::mutex titleformat_mutex;
  void publish_titleformatting_subscriptions();

  // windowing
  mpv_container* container;
  void update_title();
  std::string get_background_color();
  void set_background();

  LRESULT on_create(LPCREATESTRUCT lpcreate);
  BOOL on_erase_bg(CDCHandle dc);
  void on_context_menu(CWindow wnd, CPoint point);
  void on_destroy();

  void update();
  bool contained_in(mpv_container* p_container);

  bool find_window();
  void on_mouse_move(UINT, CPoint);

 public:
  mpv_player();
  ~mpv_player();
  static void on_containers_change();
  static void add_menu_items(uie::menu_hook_impl& menu_hook);
  static void on_new_artwork();
  static void send_message(UINT msg, UINT wparam, UINT lparam);
  static void get_title(pfc::string8& out);
  static void toggle_fullscreen();
  static void fullscreen_on_monitor(int monitor);

  static void restart();

  // window
  DECLARE_WND_CLASS_EX(TEXT("{67AAC9BC-4C35-481D-A3EB-2E2DB9727E0B}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  static DWORD GetWndStyle(DWORD style) { return WS_CHILD | WS_VISIBLE; }

  BEGIN_MSG_MAP_EX(CMpvWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_CONTEXTMENU(on_context_menu)
  MSG_WM_MOUSEMOVE(on_mouse_move)
  END_MSG_MAP()
};
}  // namespace mpv
