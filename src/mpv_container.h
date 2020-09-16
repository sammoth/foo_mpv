#pragma once
#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"

namespace mpv {
class mpv_container {
  bool pinned = false;

 public:
  long cx = 0;
  long cy = 0;
  void container_resize(long cx, long cy);
  void update_player_window();
  void container_create();
  void container_destroy();
  bool container_is_on();
  void container_pin();
  void container_unpin();
  bool container_is_pinned();
  void container_toggle_fullscreen();
  void container_on_context_menu(CWindow wnd, CPoint point);

  t_ui_color get_bg();

  virtual HWND container_wnd() = 0;
  virtual bool is_visible() = 0;
  virtual bool is_popup() = 0;
  virtual void on_fullscreen(bool fullscreen) = 0;
  virtual void invalidate() = 0;
  virtual void add_menu_items(CMenu* menu,
                              CMenuDescriptionHybrid* menudesc) = 0;
  virtual void handle_menu_cmd(int cmd) = 0;
};

mpv_container* get_main_container();
void invalidate_all_containers();
}  // namespace mpv
