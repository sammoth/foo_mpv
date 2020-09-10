#include "stdafx.h"
// PCH ^

#include <helpers/BumpableElem.h>

#include <sstream>

#include "libmpv.h"
#include "resource.h"

void RunMpvPopupWindow();

namespace mpv {
static const GUID guid_mpv_dui_panel = {
    0x777a523a, 0x1ed, 0x48b9, {0xb9, 0x1, 0xda, 0xb1, 0xbe, 0x31, 0x7c, 0xa4}};

struct CMpvDuiWindow : public ui_element_instance,
                       public mpv_container,
                       CWindowImpl<CMpvDuiWindow> {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{9D6179F4-0A94-4F76-B7EB-C4A853853DCB}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP_EX(CMpvDuiWindow)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_SIZE(on_size)
  MSG_WM_DESTROY(on_destroy)
  MSG_WM_CONTEXTMENU(on_context_menu)
  END_MSG_MAP()

  CMpvDuiWindow(ui_element_config::ptr config,
                ui_element_instance_callback_ptr p_callback)
      : m_callback(p_callback),
        m_config(config),
        background_color_enabled(false),
        background_color(0) {}

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(background_color_enabled
                                          ? background_color
                                          : m_callback->query_std_color(
                                                ui_color_background)) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));
    return TRUE;
  }

  void initialize_window(HWND parent) {
    WIN32_OP(Create(parent, 0, 0, WS_CHILD, 0));
    apply_configuration();
    create();
  }

  void on_destroy() { destroy(); }

  void on_size(UINT wparam, CSize size) { resize(size.cx, size.cy); }

  double priority() override { return x * y; }

  void on_fullscreen(bool fullscreen) override {}

  HWND container_wnd() override { return m_hWnd; }

  bool is_visible() override { return m_callback->is_elem_visible_(this); }

  HWND get_wnd() { return m_hWnd; }

  void apply_configuration() {
    try {
      ::ui_element_config_parser in(m_config);
      bool cfg_pinned;
      in >> cfg_pinned;
      if (cfg_pinned) {
        pin();
      }
      in >> background_color_enabled;
      in >> background_color;
    } catch (exception_io_data) {
    }
  };

  void set_configuration(ui_element_config::ptr config) {
    m_config = config;
    apply_configuration();
  }

  ui_element_config::ptr get_configuration() {
    ui_element_config_builder out;
    out << is_pinned();
    out << background_color_enabled;
    out << background_color;
    return out.finish(g_get_guid());
  }

  static GUID g_get_guid() { return guid_mpv_dui_panel; }
  static GUID g_get_subclass() { return ui_element_subclass_utility; }
  static void g_get_name(pfc::string_base& out) { out = "mpv Video"; }
  static ui_element_config::ptr g_get_default_configuration() {
    ui_element_config_builder out;
    out << false;
    out << false;
    out << (t_ui_color)0;
    return out.finish(g_get_guid());
  }
  static const char* g_get_description() { return "mpv Video"; }
  void notify(const GUID& p_what, t_size p_param1, const void* p_param2,
              t_size p_param2size) {
    if (p_what == ui_element_notify_visibility_changed) {
      update();
    }

    if (p_what == ui_element_notify_colors_changed ||
        p_what == ui_element_notify_font_changed) {
      // we use global colors and fonts - trigger a repaint whenever these
      // change.
      Invalidate();
    }
  };

  enum {
    ID_PIN = 1003,
    ID_POPOUT = 1004,
    ID_SETCOLOR = 1005,
    ID_RESETCOLOR = 1006,
    ID_SPLITTERCOLOR = 1007,
  };

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
    menu->AppendMenu(is_pinned() ? MF_CHECKED : MF_UNCHECKED, ID_PIN,
                     _T("Pin here"));
    menudesc->Set(ID_PIN, "Pin the video to this container");

    if (is_on()) {
      menu->AppendMenu(MF_DEFAULT, ID_POPOUT, _T("Pop out"));
      menudesc->Set(ID_POPOUT, "Open video in popup");
    }

    menu->AppendMenu(MF_SEPARATOR);
    menu->AppendMenu(MF_DEFAULT, ID_SETCOLOR, _T("Set background color"));
    menudesc->Set(ID_SETCOLOR,
                  "Choose the background color for this UI element");

    menu->AppendMenu(MF_DEFAULT, ID_RESETCOLOR, _T("Use default color"));
    menudesc->Set(ID_RESETCOLOR, "Use the default UI element background");

    menu->AppendMenu(MF_DEFAULT, ID_SPLITTERCOLOR, _T("Use splitter color"));
    menudesc->Set(ID_SPLITTERCOLOR, "Use the splitter background color");
  }

  void handle_menu_cmd(int cmd) {
    CHOOSECOLOR cc = {};
    static COLORREF acrCustClr[16];

    switch (cmd) {
      case ID_PIN:
        if (is_pinned()) {
          unpin();
        } else {
          pin();
        }
        break;
      case ID_POPOUT:
        unpin();
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
          update();
        }
        break;
      case ID_RESETCOLOR:
        background_color_enabled = false;
        Invalidate();
        update();
        break;
      case ID_SPLITTERCOLOR:
        background_color_enabled = true;
        background_color = GetSysColor(COLOR_BTNFACE);
        Invalidate();
        update();
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

  void request_activation() override { m_callback->request_activation(this); }

  t_ui_color get_background_color() override {
    return background_color_enabled ? background_color
                             : m_callback->query_std_color(ui_color_background);
  }

 private:
  ui_element_config::ptr m_config;
  t_ui_color background_color;
  bool background_color_enabled;

 protected:
  const ui_element_instance_callback_ptr m_callback;
};

class ui_element_mpvimpl : public ui_element_impl<CMpvDuiWindow> {};
static service_factory_single_t<ui_element_mpvimpl>
    g_ui_element_mpvimpl_factory;
}  // namespace mpv
