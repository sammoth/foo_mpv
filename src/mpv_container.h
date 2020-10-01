#pragma once
#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"

void RunMpvFullscreenWindow(bool reopen_popup);

namespace mpv {
class mpv_container {
 public:
  long cx = 0;
  long cy = 0;
  void on_resize(long cx, long cy);
  void on_create();
  void on_destroy();
  void pin();
  void unpin();
  bool is_pinned();
  bool owns_player();
  void on_context_menu(CWindow wnd, CPoint point);

  t_ui_color get_bg();

  virtual HWND container_wnd() = 0;

  virtual bool is_visible() = 0;
  virtual bool is_popup() = 0;
  virtual bool is_fullscreen() { return false; };

  virtual void on_gain_player(){};
  virtual void on_lose_player(){};

  virtual void toggle_fullscreen() { RunMpvFullscreenWindow(false); };

  virtual void invalidate() = 0;

  virtual void add_menu_items(CMenu* menu,
                              CMenuDescriptionHybrid* menudesc) = 0;
  virtual void handle_menu_cmd(int cmd) = 0;
  virtual bool is_osc_enabled() = 0;

  static mpv_container* get_main_container();
  static void invalidate_all_containers();
};
}  // namespace mpv
