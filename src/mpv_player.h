#pragma once
#include "stdafx.h"
// PCH ^
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <queue>
#include <sstream>
#include <thread>

#include "libmpv.h"
#include "mpv_container.h"

namespace mpv {

void get_popup_title(pfc::string8& s);

class mpv_player : ui_selection_callback_impl_base,
                   metadb_io_callback_dynamic_impl_base,
                   public CWindowImpl<mpv_player> {
  // player instance handle
  libmpv::mpv_handle* mpv_handle;
  HWND mpv_window_hwnd;
  bool enabled;

  // start mpv within the window
  bool mpv_init();
  std::mutex init_mutex;

  // thread for dispatching libmpv events
  std::thread event_listener;
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
    LoadArtwork
  };
  struct task {
    task_type type;
    metadb_handle_ptr play_file;
    double time;
    bool flag;
  };
  std::thread control_thread;
  std::condition_variable control_thread_cv;
  std::atomic_bool running_ffs;
  std::deque<task> task_queue;
  void queue_task(task t);
  bool check_queue_any();
  bool check_queue_time_change_locking();

  // methods run off thread
  void play(metadb_handle_ptr metadb, double start_time);
  void stop();
  void pause(bool p_state);
  void seek(double time, bool is_hard_sync);
  void sync(double debug_time);
  void initial_sync();
  void load_artwork();

  // state tracking
  std::atomic<double>
      time_base;  // start time of the current track/subsong within its file
  std::atomic<double> last_mpv_seek;
  std::atomic_bool sync_on_unpause;
  double last_hard_sync;
  std::mutex sync_lock;
  long last_sync_time;
  bool apply_seek_offset = false;

  std::vector<pfc::string8> profiles;

  // utils
  const char* get_string(const char* name);
  bool get_bool(const char* name);
  double get_double(const char* name);
  int command_string(const char* args);
  int set_option_string(const char* name, const char* data);
  int set_property_string(const char* name, const char* data);
  int get_property(const char* name, libmpv::mpv_format format, void* data);
  int command(const char** args);
  int set_option(const char* name, libmpv::mpv_format format, void* data);

  // scripting/artwork
  void on_selection_changed(metadb_handle_list_cref) override;
  void on_changed_sorted(metadb_handle_list_cref, bool) override;
  metadb_handle_ptr current_display_item;
  void set_display_item(metadb_handle_ptr item);
  struct titleformat_subscription {
    pfc::string8 id;
    titleformat_object::ptr object;
  };
  std::vector<titleformat_subscription> titleformat_subscriptions;
  void publish_titleformatting_subscriptions();

  // windowing
  mpv_container* container;
  void update_title();
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

  // play callbacks
  static void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused);
  static void on_playback_new_track(metadb_handle_ptr p_track);
  static void on_playback_stop(play_control::t_stop_reason p_reason);
  static void on_playback_seek(double p_time);
  static void on_playback_pause(bool p_state);
  static void on_playback_time(double p_time);
  static void on_volume_change(float new_vol);

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
