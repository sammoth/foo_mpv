#pragma once
#include "stdafx.h"
// PCH ^

#include "player.h"
#include "player_container.h"

namespace mpv {
class player_window : ui_selection_callback_impl_base,
                      metadb_io_callback_dynamic_impl_base,
                      public CWindowImpl<player_window> {

  // state
  bool video_enabled;
  player_container* container;
  t_ui_color background;

  LRESULT on_create(LPCREATESTRUCT lpcreate);
  BOOL on_erase_bg(CDCHandle dc);
  void on_context_menu(CWindow wnd, CPoint point);
  void on_destroy();
  bool find_window();
  void on_mouse_move(UINT, CPoint);

  void on_selection_changed(metadb_handle_list_cref) override;
  void on_changed_sorted(metadb_handle_list_cref, bool) override;

  void update_title();
  void update();
  bool contained_in(player_container* p_container);

 public:
  player_window(player_container* container);
  ~player_window();

  static void on_containers_change();
  static void add_menu_items(uie::menu_hook_impl& menu_hook);
  static void on_new_artwork();
  static void send_message(UINT msg, UINT wparam, UINT lparam);
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