#pragma once
#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"
#include "columns_ui-sdk/ui_extension.h"

namespace mpv {
class player_container {
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

  virtual void toggle_fullscreen();

  virtual void invalidate() = 0;
  virtual void set_title(pfc::string8 title){};

  virtual void add_menu_items(uie::menu_hook_impl& menu_hook) = 0;
  virtual bool is_osc_enabled() = 0;

  static player_container* get_main_container();
  static void invalidate_all_containers();
};
}  // namespace mpv
