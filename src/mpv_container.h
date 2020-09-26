#pragma once
#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"

void RunMpvFullscreenWindow(bool reopen_popup);

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
  void container_on_context_menu(CWindow wnd, CPoint point);

  t_ui_color get_bg();

  virtual HWND container_wnd() = 0;
  virtual bool is_visible() = 0;
  virtual bool is_popup() = 0;
  virtual bool is_fullscreen() { return false; };
  virtual void notify_pinned_elsewhere() {};
  virtual void toggle_fullscreen() {
    if (is_fullscreen()) {
      // fullscreen containers are temporary
      container_destroy();
    } else {
      RunMpvFullscreenWindow(false);
    }
  };
  virtual void invalidate() = 0;
  virtual void add_menu_items(CMenu* menu,
                              CMenuDescriptionHybrid* menudesc) = 0;
  virtual void handle_menu_cmd(int cmd) = 0;
};

mpv_container* get_main_container();
void invalidate_all_containers();
void mpv_on_new_artwork();
}  // namespace mpv
