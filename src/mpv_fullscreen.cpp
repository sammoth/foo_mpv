#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "mpv_container.h"
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

  CMpvFullscreenWindow(bool p_reopen_popup) : reopen_popup(p_reopen_popup) {}

  BEGIN_MSG_MAP(CMpvFullscreenWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_SIZE(on_size)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_KEYDOWN(on_keydown)
  MSG_WM_LBUTTONDBLCLK(on_double_click)
  MSG_WM_CONTEXTMENU(container_on_context_menu)
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
    mpv::get_popup_title(title);
    uSetWindowText(m_hWnd, title);
  }

  void on_playback_event() override { update_title(); }

  LRESULT on_create(LPCREATESTRUCT st) {
    SetClassLong(get_wnd(), GCL_HICON,
                 (LONG)LoadIcon(core_api::get_my_instance(),
                                MAKEINTRESOURCE(IDI_ICON1)));

    update_title();

    // with some window managers under wine, the window doesn't
    // properly go fullscreen unless it is resized while visible
    ShowWindow(SW_SHOW);
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST),
                    &monitor_info);
    SetWindowPos(NULL, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

    container_create();

    return 0;
  }

  void on_destroy() {
    if (reopen_popup) {
      RunMpvPopupWindow();
    }
    container_destroy();
    g_open_mpv_fullscreen = NULL;
  }

  void on_size(UINT wparam, CSize size) { container_resize(size.cx, size.cy); }

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {}

  void handle_menu_cmd(int cmd) {}

  HWND get_wnd() { return m_hWnd; }

  void on_keydown(UINT key, WPARAM, LPARAM) {
    switch (key) {
      case VK_ESCAPE:
        DestroyWindow();
      default:
        break;
    }
  }

  bool is_fullscreen() override { return true; }

  void toggle_fullscreen() override { DestroyWindow(); }

  void on_double_click(UINT, CPoint) { toggle_fullscreen(); }

  HWND container_wnd() override { return get_wnd(); }
  bool is_visible() override { return !IsIconic(); }
  bool is_popup() override { return true; }
  void invalidate() override { Invalidate(); }

 private:
  bool reopen_popup;

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

void RunMpvFullscreenWindow(bool reopen_popup) {
  if (g_open_mpv_fullscreen != NULL) {
    g_open_mpv_fullscreen->BringWindowToTop();
    return;
  }

  try {
    g_open_mpv_fullscreen =
        new CWindowAutoLifetime<CMpvFullscreenWindow>(NULL, reopen_popup);
  } catch (std::exception const& e) {
    popup_message::g_complain("Fullscreen window creation failure", e);
  }
}
