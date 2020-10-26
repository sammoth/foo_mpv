#pragma once

#include "stdafx.h"
// PCH ^
#include "player_container.h"

namespace mpv {
class fullscreen_window : public CWindowImpl<fullscreen_window>,
                          public player_container {
  bool reopen_popup_on_close;
  MONITORINFO monitor_info;

 public:
  DECLARE_WND_CLASS_EX(TEXT("{1559A84E-8A42-4C06-A515-E8D61CEBB92A}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  fullscreen_window(bool p_reopen_popup, MONITORINFO p_monitor);

  BEGIN_MSG_MAP(fullscreen_window)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_SIZE(on_size)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_KEYDOWN(on_keydown)
  MSG_WM_KEYUP(on_keyup)
  MSG_WM_SYSKEYDOWN(on_syskeydown)
  MSG_WM_SYSKEYUP(on_syskeyup)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()

  static DWORD GetWndStyle(DWORD style);
  BOOL on_erase_bg(CDCHandle dc);
  bool is_osc_enabled() override;
  LRESULT on_create(LPCREATESTRUCT st);
  void on_destroy();
  void on_size(UINT wparam, CSize size);
  void add_menu_items(uie::menu_hook_impl& hook) override;
  HWND get_wnd();
  void on_keydown(UINT wp, UINT l, UINT h);
  void on_keyup(UINT wp, UINT l, UINT h);
  void on_syskeydown(UINT wp, UINT l, UINT h);
  void on_syskeyup(UINT wp, UINT l, UINT h);
  bool is_fullscreen() override;
  void toggle_fullscreen() override;
  HWND container_wnd() override;
  bool is_visible() override;
  bool is_popup() override;
  void invalidate() override;
  void set_title(pfc::string8 title) override;

  static void open(bool reopen_popup, MONITORINFO monitor_info);
  static void close();
};
}  // namespace mpv
