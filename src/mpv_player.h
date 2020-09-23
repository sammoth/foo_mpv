#pragma once
#include "stdafx.h"
// PCH ^
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <sstream>
#include <thread>

#include "libmpv.h"
#include "mpv_container.h"

namespace mpv {

void get_popup_title(pfc::string8& s);

class mpv_player : play_callback_impl_base, public CWindowImpl<mpv_player> {
  // player
  mpv_handle* mpv;
  bool enabled;

  bool mpv_init();
  void mpv_terminate();
  void mpv_play(metadb_handle_ptr metadb, bool new_track);
  void mpv_stop();
  void mpv_pause(bool state);
  void mpv_seek(double time);
  void mpv_sync(double debug_time);

  // utils
  bool is_idle();
  const char* get_string(const char* name);
  bool get_bool(const char* name);
  double get_double(const char* name);

  std::atomic<double>
      time_base;  // start time of the current track/subsong within its file
  std::atomic<double> last_mpv_seek;
  std::atomic_bool sync_on_unpause;

  double last_hard_sync;

  std::thread sync_thread;
  std::condition_variable cv;
  std::mutex cv_mutex;
  enum class sync_task_type { Wait, Stop, Quit, FirstFrameSync };
  std::atomic<sync_task_type> sync_task;
  void sync_thread_sync();

  // windowing
  mpv_container* container;
  void update_container();
  void update_window();
  void update_title();
  void set_background();

  // callbacks
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused);
  void on_playback_new_track(metadb_handle_ptr p_track);
  void on_playback_stop(play_control::t_stop_reason p_reason);
  void on_playback_seek(double p_time);
  void on_playback_pause(bool p_state);
  void on_playback_time(double p_time);

  LRESULT on_create(LPCREATESTRUCT lpcreate);
  BOOL on_erase_bg(CDCHandle dc);
  void on_context_menu(CWindow wnd, CPoint point);
  void on_double_click(UINT, CPoint);
  void on_destroy();

 public:
  mpv_player();
  ~mpv_player();

  void update();
  void destroy();
  bool contained_in(mpv_container* container);

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc);
  void handle_menu_cmd(int cmd);

  // window
  DECLARE_WND_CLASS_EX(TEXT("{67AAC9BC-4C35-481D-A3EB-2E2DB9727E0B}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  static DWORD GetWndStyle(DWORD style) {
    return WS_CHILD | WS_VISIBLE;
  }

  BEGIN_MSG_MAP_EX(CMpvWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_LBUTTONDBLCLK(on_double_click)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()
};
}  // namespace mpv
