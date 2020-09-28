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

class mpv_player : play_callback_impl_base,
                   ui_selection_callback_impl_base,
                   public CWindowImpl<mpv_player> {
  // player
  std::unique_ptr<mpv_handle, decltype(libmpv()->terminate_destroy)> mpv_handle;
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
  std::queue<task> task_queue;
  void queue_task(task t);
  bool check_queue_any();

  // methods run off thread
  void play(metadb_handle_ptr metadb, double start_time);
  void stop();
  void pause(bool p_state);
  void seek(double time);
  void sync(double debug_time);
  void initial_sync();
  void load_artwork();

  // state tracking
  std::atomic<double>
      time_base;  // start time of the current track/subsong within its file
  std::atomic<double> last_mpv_seek;
  std::atomic_bool sync_on_unpause;
  double last_hard_sync;

  // utils
  bool is_idle();
  const char* get_string(const char* name);
  bool get_bool(const char* name);
  double get_double(const char* name);
  int command_string(const char* args);
  int set_option_string(const char* name, const char* data);
  int set_property_string(const char* name, const char* data);
  int get_property(const char* name, mpv_format format, void* data);
  int command(const char** args);
  int set_option(const char* name, mpv_format format, void* data);

  // play callbacks
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused);
  void on_playback_new_track(metadb_handle_ptr p_track);
  void on_playback_stop(play_control::t_stop_reason p_reason);
  void on_playback_seek(double p_time);
  void on_playback_pause(bool p_state);
  void on_playback_time(double p_time);

  // artwork
  void on_selection_changed(metadb_handle_list_cref p_selection) override;

  // windowing
  mpv_container* container;
  void update_title();
  void set_background();

  LRESULT on_create(LPCREATESTRUCT lpcreate);
  BOOL on_erase_bg(CDCHandle dc);
  void on_context_menu(CWindow wnd, CPoint point);
  void on_double_click(UINT, CPoint);
  void on_destroy();

  void update();
  void destroy();
  bool contained_in(mpv_container* p_container);

 public:
  mpv_player();
  ~mpv_player();
  static void on_containers_change();
  static void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc);
  static void handle_menu_cmd(int cmd);
  static void on_new_artwork();

  // window
  DECLARE_WND_CLASS_EX(TEXT("{67AAC9BC-4C35-481D-A3EB-2E2DB9727E0B}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  static DWORD GetWndStyle(DWORD style) { return WS_CHILD | WS_VISIBLE; }

  BEGIN_MSG_MAP_EX(CMpvWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_LBUTTONDBLCLK(on_double_click)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()
};
}  // namespace mpv
