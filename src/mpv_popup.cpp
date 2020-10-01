#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "mpv_container.h"
#include "preferences.h"
#include "resource.h"

void RunMpvPopupWindow();
void RunMpvFullscreenWindow(bool reopen_popup);

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
  MSG_WM_LBUTTONDBLCLK(on_double_click)
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
    mpv::get_popup_title(title);
    uSetWindowText(m_hWnd, title);
  }

  void on_playback_event() override { update_title(); }

  void toggle_fullscreen() override { RunMpvFullscreenWindow(true); };

  void on_lose_player() override { DestroyWindow(); }

  bool is_osc_enabled() override { return true; }

  LRESULT on_create(LPCREATESTRUCT st) {
    if (cfg_mpv_popup_separate) {
      SetWindowLongPtr(GWLP_HWNDPARENT, NULL);
    } else {
      SetWindowLongPtr(GWLP_HWNDPARENT, (LONG_PTR)core_api::get_main_window());
    }

    SetClassLong(get_wnd(), GCL_HICON,
                 (LONG)LoadIcon(core_api::get_my_instance(),
                                MAKEINTRESOURCE(IDI_ICON1)));

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

  void on_size(UINT wparam, CSize size) { mpv_container::on_resize(size.cx, size.cy); }

  enum {
    ID_UNPIN = 1003,
    ID_SETCOLOUR = 1004,
    ID_SEPARATE = 1005,
    ID_ONTOP = 1006,
    ID_SEP=9999,
  };

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
    menu->AppendMenu(MF_SEPARATOR, ID_SEP, _T(""));
    menu->AppendMenu(cfg_mpv_popup_alwaysontop ? MF_CHECKED : MF_UNCHECKED,
                     ID_ONTOP, _T("Always on-top"));
    menudesc->Set(ID_ONTOP, "Keep the video window above other windows");
    menu->AppendMenu(cfg_mpv_popup_separate ? MF_CHECKED : MF_UNCHECKED,
                     ID_SEPARATE, _T("Separate from main window"));
    menudesc->Set(ID_SEPARATE,
                  "Allow window to separate from the foobar2000 main window");
  }

  void handle_menu_cmd(int cmd) {
    RECT client_rect = {};
    switch (cmd) {
      case ID_ONTOP:
        cfg_mpv_popup_alwaysontop = !cfg_mpv_popup_alwaysontop;
        DestroyWindow();
        RunMpvPopupWindow();
        break;
      case ID_SEPARATE:
        cfg_mpv_popup_separate = !cfg_mpv_popup_separate;
        DestroyWindow();
        RunMpvPopupWindow();
        break;
      case ID_UNPIN:
        unpin();
        break;
      default:
        break;
    }
  }

  HWND get_wnd() { return m_hWnd; }

  void on_double_click(UINT, CPoint) { toggle_fullscreen(); }

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
