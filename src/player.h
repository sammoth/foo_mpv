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
#include "player_container.h"

namespace mpv {

class player {
  enum class mpv_state_t {
    Unloaded,
    Preload,
    Loading,
    Active,
    Idle,
    Seeking,
    Artwork,
    Shutdown
  };

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

  const HWND parent_window_hwnd;
  HWND mpv_window_hwnd;
  // start mpv within the window
  std::mutex init_mutex;

  // player instance handle
  libmpv::mpv_handle* mpv_handle;

  // thread for dispatching libmpv events
  std::thread event_listener;
  std::condition_variable event_cv;
  std::mutex mutex;
  std::atomic<double> mpv_timepos;
  std::atomic<mpv_state_t> mpv_state;
  void set_state(mpv_state_t new_state);

  // thread for deferred player control tasks
  std::thread control_thread;
  std::condition_variable control_thread_cv;
  std::deque<task> task_queue;
  bool check_queue_any();
  bool check_queue_time_change_locking();

  // tasks
  void run_play(metadb_handle_ptr metadb, double start_time);
  void run_stop();
  void run_pause(bool p_state);
  void run_seek(double time, bool is_hard_sync);
  void run_initial_sync();
  void run_load_artwork();

  // state tracking
  std::atomic_bool running_initial_sync;
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

  metadb_handle_ptr current_display_item;
  void set_display_item(metadb_handle_ptr item);

  struct titleformat_subscription {
    pfc::string8 id;
    titleformat_object::ptr object;
  };
  std::vector<titleformat_subscription> titleformat_subscriptions;
  void publish_titleformatting_subscriptions();

  void set_background();

  bool init();

 public:
  player(HWND parent);
  ~player();

  void queue_task(task t);
  void activate_window();
  void publish_title(pfc::string8);
  void sync(double debug_time);
  void play(metadb_handle_ptr metadb, double start_time);
  void stop();
  void pause(bool p_state);
  void seek(double time, bool is_hard_sync);
  void load_artwork();
  void starting(bool paused);
  void set_volume(float vol);

  void add_menu_items(uie::menu_hook_impl& menu_hook);
  static bool should_play_this(metadb_handle_ptr metadb, pfc::string8& out);

  static std::unique_ptr<player> create(HWND parent);
};
}  // namespace mpv
