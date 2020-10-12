#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "menu_utils.h"
#include "mpv_container.h"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"

void RunMpvPopupWindow();
void RunMpvFullscreenWindow(bool reopen_popup, MONITORINFO monitor);

namespace {
struct CMpvPopupWindow;
static CMpvPopupWindow* g_open_mpv_popup;

static const GUID guid_cfg_mpv_popup_rect = {
    0x6f8a673, 0x3861, 0x43fb, {0xa4, 0xfe, 0xe8, 0xdf, 0xc7, 0x1, 0x45, 0x70}};
static const GUID guid_cfg_mpv_popup_alwaysontop = {
    0xc93067e,
    0xaa41,
    0x4145,
    {0x8d, 0x75, 0x3f, 0xab, 0x81, 0xf5, 0xcb, 0xf1}};
static const GUID guid_cfg_mpv_popup_separate = {
    0x52b44a9e,
    0x202d,
    0x4e23,
    {0xac, 0x24, 0xab, 0x34, 0x78, 0x65, 0x8b, 0xa5}};

static cfg_struct_t<RECT> cfg_mpv_popup_rect(guid_cfg_mpv_popup_rect, 0);
static cfg_bool cfg_mpv_popup_alwaysontop(guid_cfg_mpv_popup_alwaysontop,
                                          false);
static cfg_bool cfg_mpv_popup_separate(guid_cfg_mpv_popup_separate, true);

struct CMpvPopupWindow : public CWindowImpl<CMpvPopupWindow>,
                         public mpv::mpv_container,
                         public playback_event_notify {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{1559A84E-8A42-4C06-A515-E8D61CEBB92A}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP(CMpvPopupWindow)
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
    return WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  }

  static DWORD GetWndExStyle(DWORD dwExStyle) {
    if (cfg_mpv_popup_alwaysontop) {
      return WS_EX_TOPMOST;
    } else {
      return 0;
    }
  };

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

  void on_playback_event() override { update_title(); }

  void toggle_fullscreen() override {
    MONITORINFO monitor;
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(
        MonitorFromWindow(container_wnd(), MONITOR_DEFAULTTONEAREST), &monitor);

    RunMpvFullscreenWindow(true, monitor);
  };

  void on_lose_player() override { DestroyWindow(); }

  bool is_osc_enabled() override { return true; }

  LRESULT on_create(LPCREATESTRUCT st) {
    if (cfg_mpv_popup_separate) {
      SetWindowLongPtr(GWLP_HWNDPARENT, NULL);
    } else {
      SetWindowLongPtr(GWLP_HWNDPARENT, (LONG_PTR)core_api::get_main_window());
    }

    SetClassLong(get_wnd(), GCL_HICON,
                 (LONG)ui_control::get()->get_main_icon());

    update_title();

    RECT rect = cfg_mpv_popup_rect;
    if (standard_config_objects::query_remember_window_positions() &&
            rect.bottom != 0 ||
        rect.right != 0 || rect.left != 0 || rect.top != 0) {
      SetWindowPos(NULL, &rect, SWP_NOZORDER | SWP_FRAMECHANGED);
    } else {
      MONITORINFO monitor_info;
      monitor_info.cbSize = sizeof(monitor_info);
      GetMonitorInfoW(MonitorFromWindow(get_wnd(), MONITOR_DEFAULTTONEAREST),
                      &monitor_info);
      long height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
      long width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
      SetWindowPos(NULL, monitor_info.rcMonitor.left + width / 4,
                   monitor_info.rcMonitor.top + height / 4, width / 2,
                   height / 2, SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    mpv_container::on_create();

    unpin();

    return 0;
  }

  void on_destroy() {
    RECT client_rect = {};
    GetWindowRect(&client_rect);
    cfg_mpv_popup_rect = client_rect;
    mpv_container::on_destroy();
    g_open_mpv_popup = NULL;
  }

  void on_size(UINT wparam, CSize size) {
    mpv_container::on_resize(size.cx, size.cy);
  }

  void add_menu_items(uie::menu_hook_impl& menu_hook) override {
    if (menu_hook.get_children_count() > 0) {
      menu_hook.add_node(new uie::menu_node_separator_t());
    }
    menu_hook.add_node(new menu_utils::menu_node_run(
        "Always on-top", "Keep the video window above other windows",
        cfg_mpv_popup_alwaysontop, [this]() {
          cfg_mpv_popup_alwaysontop = !cfg_mpv_popup_alwaysontop;
          DestroyWindow();
          RunMpvPopupWindow();
        }));
    menu_hook.add_node(new menu_utils::menu_node_run(
        "Separate from main window",
        "Allow window to separate from the foobar2000 main window",
        cfg_mpv_popup_separate, [this]() {
          cfg_mpv_popup_separate = !cfg_mpv_popup_separate;
          DestroyWindow();
          RunMpvPopupWindow();
        }));
  }

  HWND get_wnd() { return m_hWnd; }

  HWND container_wnd() override { return get_wnd(); }
  bool is_visible() override { return !IsIconic(); }
  bool is_popup() override { return true; }
  void invalidate() override { Invalidate(); }

 private:
 protected:
};

class close_popup_handler : public initquit {
 public:
  void on_quit() override {
    if (g_open_mpv_popup != NULL) {
      g_open_mpv_popup->DestroyWindow();
    }
  }
};

static initquit_factory_t<close_popup_handler> popup_closer;
}  // namespace

void RunMpvPopupWindow() {
  if (g_open_mpv_popup != NULL) {
    g_open_mpv_popup->BringWindowToTop();
    return;
  }

  try {
    g_open_mpv_popup = new CWindowAutoLifetime<CMpvPopupWindow>(NULL);
  } catch (std::exception const& e) {
    popup_message::g_complain("Popup creation failure", e);
  }
}
