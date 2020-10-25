#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "fullscreen_window.h"
#include "player.h"
#include "player_container.h"
#include "popup_window.h"
#include "preferences.h"
#include "resource.h"

namespace mpv {
static fullscreen_window* g_fullscreen_window;

fullscreen_window::fullscreen_window(bool p_reopen_popup, MONITORINFO p_monitor)
    : reopen_popup_on_close(p_reopen_popup), monitor_info(p_monitor) {}

DWORD fullscreen_window::GetWndStyle(DWORD style) {
  return WS_POPUP | WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU;
}

BOOL fullscreen_window::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(get_bg()) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));

  return TRUE;
}

void fullscreen_window::set_title(pfc::string8 title) {
  if (g_fullscreen_window) {
    uSetWindowText(g_fullscreen_window->m_hWnd, title);
  }
}

bool fullscreen_window::is_osc_enabled() { return true; }

LRESULT fullscreen_window::on_create(LPCREATESTRUCT st) {
  SetClassLong(get_wnd(), GCL_HICON, (LONG)ui_control::get()->get_main_icon());

  pfc::string8 title;
  player::get_title(title);
  set_title(title);

  // with some window managers under wine, the window doesn't
  // properly go fullscreen unless it is resized while visible
  ShowWindow(SW_SHOW);
  SetWindowPos(NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
               monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
               monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
               SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

  player_container::on_create();

  return 0;
}

void fullscreen_window::on_destroy() {
  if (reopen_popup_on_close) {
    popup_window::open();
  }
  player_container::on_destroy();
  g_fullscreen_window = NULL;
}

void fullscreen_window::on_size(UINT wparam, CSize size) {
  player_container::on_resize(size.cx, size.cy);
}

void fullscreen_window::add_menu_items(uie::menu_hook_impl& hook) {}

HWND fullscreen_window::get_wnd() { return m_hWnd; }

void fullscreen_window::on_keydown(UINT wp, UINT l, UINT h) {
  switch (wp) {
    case VK_ESCAPE:
      DestroyWindow();
    default:
      mpv::player::send_message(WM_KEYDOWN, wp, MAKELPARAM(l, h));
      break;
  }
}

void fullscreen_window::on_keyup(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_KEYUP, wp, MAKELPARAM(l, h));
}

void fullscreen_window::on_syskeydown(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_SYSKEYDOWN, wp, MAKELPARAM(l, h));
}

void fullscreen_window::on_syskeyup(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_SYSKEYUP, wp, MAKELPARAM(l, h));
}

bool fullscreen_window::is_fullscreen() { return true; }

void fullscreen_window::toggle_fullscreen() {
  fb2k::inMainThread([this]() { DestroyWindow(); });
}

HWND fullscreen_window::container_wnd() { return get_wnd(); }
bool fullscreen_window::is_visible() { return !IsIconic(); }
bool fullscreen_window::is_popup() { return true; }
void fullscreen_window::invalidate() { Invalidate(); }

void fullscreen_window::open(bool reopen_popup, MONITORINFO monitor_info) {
  if (mpv::g_fullscreen_window != NULL) {
    mpv::g_fullscreen_window->SetWindowPos(
        NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
        monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
        monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    mpv::g_fullscreen_window->BringWindowToTop();
    return;
  }

  try {
    mpv::g_fullscreen_window = new CWindowAutoLifetime<mpv::fullscreen_window>(
        NULL, reopen_popup, monitor_info);
  } catch (std::exception const& e) {
    popup_message::g_complain("Fullscreen window creation failure", e);
  }
}

void fullscreen_window::close() {
  if (g_fullscreen_window) {
    g_fullscreen_window->DestroyWindow();
    g_fullscreen_window = NULL;
  }
}

class close_fullscreen_initquit : public initquit {
 public:
  void on_quit() override {
    if (g_fullscreen_window != NULL) {
      g_fullscreen_window->DestroyWindow();
    }
  }
};

static initquit_factory_t<close_fullscreen_initquit> g_fullscreen_closer;
}  // namespace mpv
