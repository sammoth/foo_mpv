#pragma once
#include "stdafx.h"
// PCH ^

#include "player_container.h"

namespace mpv {
class popup_window : public CWindowImpl<popup_window>, public player_container {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{1559A84E-8A42-4C06-A515-E8D61CEBB92A}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP(popup_window)
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
  static DWORD GetWndExStyle(DWORD dwExStyle);

  BOOL on_erase_bg(CDCHandle dc);
  void on_keydown(UINT wp, UINT l, UINT h);
  void on_keyup(UINT wp, UINT l, UINT h);
  void on_syskeydown(UINT wp, UINT l, UINT h);
  void on_syskeyup(UINT wp, UINT l, UINT h);
  LRESULT on_create(LPCREATESTRUCT st);
  void on_destroy();
  void on_size(UINT wparam, CSize size);

  HWND get_wnd() { return m_hWnd; }

  HWND container_wnd() override { return get_wnd(); }
  bool is_visible() override { return !IsIconic(); }
  bool is_popup() override { return true; }
  void invalidate() override { Invalidate(); }

  void toggle_fullscreen() override;
  void on_lose_player() override;
  bool is_osc_enabled() override;
  void add_menu_items(uie::menu_hook_impl& menu_hook) override;

  static void open();
  static void open(bool pop_existing);
  static void close();
  void set_title(pfc::string8 title) override;
};
}  // namespace mpv
