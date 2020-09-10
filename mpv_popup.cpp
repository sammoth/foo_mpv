#include "stdafx.h"
// PCH ^

#include <helpers/atl-misc.h>

#include <sstream>

#include "libmpv.h"
#include "resource.h"

namespace {
struct popup_owner {
  virtual void destroy_owner() = 0;
  virtual void pop() = 0;
};

struct CMpvPopupWindow : public CWindowImpl<CMpvPopupWindow>,
                         public mpv::mpv_container {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{1559A84E-8A42-4C06-A515-E8D61CEBB92A}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP(CMpvPopupWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_SIZE(on_size)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()

  static DWORD GetWndStyle(DWORD style) {
    return WS_POPUP | WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  }

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(0x00000000) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));
    return TRUE;
  }

  LRESULT on_create(LPCREATESTRUCT st) {
    SetClassLong(get_wnd(), GCL_HICON,
                 (LONG)LoadIcon(core_api::get_my_instance(),
                                MAKEINTRESOURCE(IDI_ICON1)));

    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(MonitorFromWindow(get_wnd(), MONITOR_DEFAULTTONEAREST),
                    &monitor_info);
    long height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
    long width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
    SetWindowPos(NULL, monitor_info.rcMonitor.left + width / 4,
                 monitor_info.rcMonitor.top + height / 4, width / 2, height / 2,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
    create();
    return 0;
  }

  void on_destroy() {
    ShowWindow(SW_HIDE);
    destroy();
    owner->destroy_owner();
  }

  void on_size(UINT wparam, CSize size) { resize(size.cx, size.cy); }

  double priority() override { return 10e8 + (double)x * (double)y; }

  enum {
    ID_UNPIN = 1003,
    ID_SETCOLOUR = 1004,
    ID_STICK = 1005,
    ID_ONTOP = 1006,
  };

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
    if (!is_on()) {
      menu->AppendMenu(ontop ? MF_CHECKED : MF_UNCHECKED, ID_UNPIN,
                       _T("Unpin"));
      menudesc->Set(ID_UNPIN, "Unpin elsewhere");
    }
    menu->AppendMenu(ontop ? MF_CHECKED : MF_UNCHECKED, ID_ONTOP,
                     _T("Always on-top"));
    menudesc->Set(ID_ONTOP, "Keep the video window above other windows");
    menu->AppendMenu(stick ? MF_CHECKED : MF_UNCHECKED, ID_STICK,
                     _T("Stick to main window"));
    menudesc->Set(ID_STICK,
                  "Minimize/restore window with foobar2000 main window and always keep above");
  }

  bool ontop = false;
  bool stick = false;

  void handle_menu_cmd(int cmd) {
    switch (cmd) {
      case ID_ONTOP:
        ontop = !ontop;
        if (ontop) {
          SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        } else {
          SetWindowPos(HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        break;
      case ID_STICK:
        stick = !stick;
        if (stick) {
          SetWindowLongPtr(GWLP_HWNDPARENT,
                           (LONG_PTR)core_api::get_main_window());
        } else {
          SetWindowLongPtr(GWLP_HWNDPARENT, NULL);
        }
        break;
      case ID_UNPIN:
        unpin();
        break;
      default:
        break;
    }
  }

  void on_context_menu(CWindow wnd, CPoint point) {
    try {
      {
        // handle the context menu key case - center the menu
        if (point == CPoint(-1, -1)) {
          CRect rc;
          WIN32_OP(wnd.GetWindowRect(&rc));
          point = rc.CenterPoint();
        }

        CMenuDescriptionHybrid menudesc(
            *this);  // this class manages all the voodoo necessary for
                     // descriptions of our menu items to show in the status
                     // bar.

        static_api_ptr_t<contextmenu_manager> api;
        CMenu menu;
        WIN32_OP(menu.CreatePopupMenu());

        add_menu_items(&menu, &menudesc);

        int cmd =
            menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                                point.x, point.y, menudesc, 0);

        handle_menu_cmd(cmd);
      }
    } catch (std::exception const& e) {
      console::complain("Context menu failure", e);  // rare
    }
  }

  HWND get_wnd() { return m_hWnd; }

  void on_fullscreen(bool fullscreen) override {
    ShowWindow(fullscreen ? SW_HIDE : SW_NORMAL);
  }

  void set_owner(popup_owner* p_owner) { owner = p_owner; }

  HWND container_wnd() override { return get_wnd(); }
  bool is_visible() override { return !IsIconic(); }
  t_ui_color get_background_color() override { return (t_ui_color)0; }
  void request_activation() override {
    ::SetActiveWindow(get_wnd());
    ::SetFocus(get_wnd());
  }

 private:
  popup_owner* owner;

 protected:
};

static popup_owner* open_owner;
struct CMpvPopupOwnerWindow : public CWindowImpl<CMpvPopupOwnerWindow>,
                              public popup_owner {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{551A3229-D929-44A2-8022-161B39CDBCDF}"), 0, (-1));

  BEGIN_MSG_MAP(CMpvPopupOwnerWindow)
  MSG_WM_CREATE(on_create)
  MSG_WM_DESTROY(on_destroy)
  END_MSG_MAP()

  static DWORD GetWndStyle(DWORD style) { return WS_CHILD | WS_EX_NOACTIVATE; }

  LRESULT on_create(LPCREATESTRUCT st) {
    child = new CWindowAutoLifetime<CMpvPopupWindow>(NULL);
    child->set_owner(this);
    return 0;
  }

  void on_destroy() {
    child->DestroyWindow();
    open_owner = NULL;
  }

  void destroy_owner() override { DestroyWindow(); }

  void pop() override { child->BringWindowToTop(); }

 private:
  CMpvPopupWindow* child;

 protected:
};
}  // namespace

void RunMpvPopupWindow() {
  if (open_owner != NULL) {
    open_owner->pop();
    return;
  }

  try {
    open_owner = new CWindowAutoLifetime<CMpvPopupOwnerWindow>(
        core_api::get_main_window());
  } catch (std::exception const& e) {
    popup_message::g_complain("Popup creation failure", e);
  }
}
