#include "stdafx.h"
// PCH ^

#include <commctrl.h>
#include <helpers/BumpableElem.h>
#include <windows.h>
#include <windowsx.h>

#include "columns_ui-sdk/ui_extension.h"
#include "foobar2000/SDK/foobar2000.h"
#include "libmpv.h"
#include "resource.h"

void RunMpvPopupWindow();

namespace mpv {
static const GUID g_guid_mpv_cui_panel = {
    0xd784f81,
    0x76c1,
    0x48e2,
    {0xa1, 0x39, 0x37, 0x6c, 0x39, 0xff, 0xe3, 0x3c}};

struct CMpvCuiWindow : public mpv_container, CWindowImpl<CMpvCuiWindow> {
  DECLARE_WND_CLASS_EX(TEXT("{9D6179F4-0A94-4F76-B7EB-C4A853853DCB}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP_EX(CMpvCuiWindow)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_CREATE(on_create)
  MSG_WM_SIZE(on_size)
  MSG_WM_SHOWWINDOW(on_show)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()

  static DWORD GetWndStyle(DWORD style) {
    return WS_CHILD | WS_VISIBLE | WS_EX_LAYOUTRTL;
  }

  CMpvCuiWindow() : background_color_enabled(false), background_color(0) {}

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(
                   background_color_enabled ? background_color : 0) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));
    return TRUE;
  }

  LRESULT on_create(LPCREATESTRUCT lp) {
    container_create();
    return 0;
  }

  void on_destroy() { container_destroy(); }

  void on_size(UINT wparam, CSize size) { container_resize(size.cx, size.cy); }

  LRESULT on_show(UINT wparam, UINT lparam) {
    container_update();
    return 0;
  }

  void on_fullscreen(bool fullscreen) override {}

  HWND container_wnd() override { return m_hWnd; }

  bool is_visible() override {
    return IsWindowVisible() && !::IsIconic(core_api::get_main_window());
  }

  bool is_popup() override { return false; }

  HWND get_wnd() { return m_hWnd; }

  enum {
    ID_PIN = 1003,
    ID_POPOUT = 1004,
    ID_SETCOLOR = 1005,
    ID_RESETCOLOR = 1006,
    ID_SPLITTERCOLOR = 1007,
  };

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
    menu->AppendMenu(container_is_pinned() ? MF_CHECKED : MF_UNCHECKED, ID_PIN,
                     _T("Pin here"));
    menudesc->Set(ID_PIN, "Pin the video to this container");

    if (container_is_on()) {
      menu->AppendMenu(MF_DEFAULT, ID_POPOUT, _T("Pop out"));
      menudesc->Set(ID_POPOUT, "Open video in popup");
    }

    menu->AppendMenu(MF_SEPARATOR);
    menu->AppendMenu(MF_DEFAULT, ID_SETCOLOR, _T("Set background color"));
    menudesc->Set(ID_SETCOLOR,
                  "Choose the background color for this UI element");

    menu->AppendMenu(MF_DEFAULT, ID_RESETCOLOR, _T("Default color"));
    menudesc->Set(ID_RESETCOLOR, "Use the default UI element background");

    menu->AppendMenu(MF_DEFAULT, ID_SPLITTERCOLOR, _T("Splitter color"));
    menudesc->Set(ID_SPLITTERCOLOR, "Use the splitter background color");
  }

  void handle_menu_cmd(int cmd) {
    CHOOSECOLOR cc = {};
    static COLORREF acrCustClr[16];

    switch (cmd) {
      case ID_PIN:
        if (container_is_pinned()) {
          container_unpin();
        } else {
          container_pin();
        }
        break;
      case ID_POPOUT:
        container_unpin();
        RunMpvPopupWindow();
        break;
      case ID_SETCOLOR:
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = get_wnd();
        cc.lpCustColors = (LPDWORD)acrCustClr;
        cc.rgbResult = background_color;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (ChooseColor(&cc) == TRUE) {
          background_color = cc.rgbResult;
          background_color_enabled = true;
          Invalidate();
          container_update();
        }
        break;
      case ID_RESETCOLOR:
        background_color_enabled = false;
        Invalidate();
        container_update();
        break;
      case ID_SPLITTERCOLOR:
        background_color_enabled = true;
        background_color = GetSysColor(COLOR_BTNFACE);
        Invalidate();
        container_update();
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

  t_ui_color get_background_color() override {
    return background_color_enabled ? background_color : 0;
  }

 private:
  t_ui_color background_color;
  bool background_color_enabled;
};

class MpvCuiWindow : public uie::container_ui_extension {
 public:
  const GUID& get_extension_guid() const override {
    return g_guid_mpv_cui_panel;
  }
  void get_name(pfc::string_base& out) const override { out = "mpv"; }
  void get_category(pfc::string_base& out) const override { out = "Panels"; }
  unsigned get_type() const override { return uie::type_panel; }

 private:
  class_data& get_class_data() const override {
    __implement_get_class_data(_T("{EF25F318-A1F7-46CB-A86E-70F568ADDCE6}"),
                               false);
  }

  LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) override;

  CWindowAutoLifetime<mpv::CMpvCuiWindow>* wnd_child;
};

LRESULT MpvCuiWindow::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      wnd_child = new CWindowAutoLifetime<CMpvCuiWindow>(wnd);
      break;
    case WM_SHOWWINDOW:
      wnd_child->ShowWindow(wp);
      break;
    case WM_SIZE:
      wnd_child->SetWindowPos(0, 0, 0, LOWORD(lp), HIWORD(lp), SWP_NOZORDER);
      break;
    case WM_DESTROY:
      wnd_child = NULL;
      break;
  }
  return DefWindowProc(wnd, msg, wp, lp);
}

static uie::window_factory<MpvCuiWindow> mpv_cui_window_factory;
}  // namespace mpv
