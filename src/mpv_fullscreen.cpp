#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "mpv_container.h"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"

void RunMpvPopupWindow();

namespace {
struct CMpvFullscreenWindow;
static CMpvFullscreenWindow* g_open_mpv_fullscreen;

struct CMpvFullscreenWindow : public CWindowImpl<CMpvFullscreenWindow>,
                              public mpv::mpv_container,
                              public playback_event_notify {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{1559A84E-8A42-4C06-A515-E8D61CEBB92A}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  CMpvFullscreenWindow(bool p_reopen_popup, MONITORINFO p_monitor)
      : reopen_popup(p_reopen_popup), monitor_info(p_monitor) {}

  BEGIN_MSG_MAP(CMpvFullscreenWindow)
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

  static DWORD GetWndStyle(DWORD style) {
    return WS_POPUP | WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU;
  }

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(get_bg()) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));

    return TRUE;
  }

  void update_title() {
    pfc::string8 title;
    mpv::mpv_player::get_title(title);
    uSetWindowText(m_hWnd, title);
  }

  void on_playback_event() override { update_title(); }

  bool is_osc_enabled() override { return true; }

  LRESULT on_create(LPCREATESTRUCT st) {
    SetClassLong(get_wnd(), GCL_HICON,
                 (LONG)ui_control::get()->get_main_icon());

    update_title();

    // with some window managers under wine, the window doesn't
    // properly go fullscreen unless it is resized while visible
    ShowWindow(SW_SHOW);
    SetWindowPos(NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

    mpv_container::on_create();

    return 0;
  }

  void on_destroy() {
    if (reopen_popup) {
      RunMpvPopupWindow();
    }
    mpv_container::on_destroy();
    g_open_mpv_fullscreen = NULL;
  }

  void on_size(UINT wparam, CSize size) {
    mpv_container::on_resize(size.cx, size.cy);
  }

  void add_menu_items(uie::menu_hook_impl& hook) override {}

  HWND get_wnd() { return m_hWnd; }

  void on_keydown(UINT wp, UINT l, UINT h) {
    switch (wp) {
      case VK_ESCAPE:
        DestroyWindow();
      default:
        mpv::mpv_player::send_message(WM_KEYDOWN, wp, MAKELPARAM(l, h));
        break;
    }
  }

  void on_keyup(UINT wp, UINT l, UINT h) {
    mpv::mpv_player::send_message(WM_KEYUP, wp, MAKELPARAM(l, h));
  }

  void on_syskeydown(UINT wp, UINT l, UINT h) {
    mpv::mpv_player::send_message(WM_SYSKEYDOWN, wp, MAKELPARAM(l, h));
  }

  void on_syskeyup(UINT wp, UINT l, UINT h) {
    mpv::mpv_player::send_message(WM_SYSKEYUP, wp, MAKELPARAM(l, h));
  }

  bool is_fullscreen() override { return true; }

  void toggle_fullscreen() override {
    fb2k::inMainThread([this]() { DestroyWindow(); });
  }

  HWND container_wnd() override { return get_wnd(); }
  bool is_visible() override { return !IsIconic(); }
  bool is_popup() override { return true; }
  void invalidate() override { Invalidate(); }

 private:
  bool reopen_popup;
  MONITORINFO monitor_info;

 protected:
};

class close_popup_handler : public initquit {
 public:
  void on_quit() override {
    if (g_open_mpv_fullscreen != NULL) {
      g_open_mpv_fullscreen->DestroyWindow();
    }
  }
};

static initquit_factory_t<close_popup_handler> popup_closer;
}  // namespace

void RunMpvFullscreenWindow(bool reopen_popup, MONITORINFO monitor_info) {
  if (g_open_mpv_fullscreen != NULL) {
    g_open_mpv_fullscreen->SetWindowPos(
        NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
        monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
        monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    g_open_mpv_fullscreen->BringWindowToTop();
    return;
  }

  try {
    g_open_mpv_fullscreen = new CWindowAutoLifetime<CMpvFullscreenWindow>(
        NULL, reopen_popup, monitor_info);
  } catch (std::exception const& e) {
    popup_message::g_complain("Fullscreen window creation failure", e);
  }
}
