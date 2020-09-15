#include "stdafx.h"
// PCH ^

#include <../helpers/BumpableElem.h>

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
  MSG_WM_LBUTTONDBLCLK(on_double_click)
  MSG_WM_CONTEXTMENU(container_on_context_menu)
  END_MSG_MAP()

  CMpvDuiWindow(ui_element_config::ptr config,
                ui_element_instance_callback_ptr p_callback)
      : m_callback(p_callback), m_config(config) {}

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(get_bg()) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));
    return TRUE;
  }

  void initialize_window(HWND parent) {
    WIN32_OP(Create(parent, 0, 0, WS_CHILD, 0));
    apply_configuration();
    container_create();
    if (m_callback->is_edit_mode_enabled()) {
      SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) |
                                     WS_EX_TRANSPARENT | WS_EX_LAYERED);
    }
  }

  void on_double_click(UINT, CPoint) { container_toggle_fullscreen(); }

  void on_size(UINT wparam, CSize size) { container_resize(size.cx, size.cy); }

  void on_destroy() { container_destroy(); }

  void on_fullscreen(bool fullscreen) override {}

  HWND container_wnd() override { return m_hWnd; }

  bool is_visible() override { return m_callback->is_elem_visible_(this); }
  bool is_popup() override { return false; }
  void invalidate() override { Invalidate(); }

  HWND get_wnd() { return m_hWnd; }

  void apply_configuration() {
    try {
      ::ui_element_config_parser in(m_config);
      bool cfg_pinned;
      in >> cfg_pinned;
      if (cfg_pinned) {
        container_pin();
      }
    } catch (exception_io_data) {
    }
  };

  void set_configuration(ui_element_config::ptr config) {
    m_config = config;
    apply_configuration();
  }

  ui_element_config::ptr get_configuration() {
    ui_element_config_builder out;
    out << container_is_pinned();
    return out.finish(g_get_guid());
  }

  static GUID g_get_guid() { return guid_mpv_dui_panel; }
  static GUID g_get_subclass() { return ui_element_subclass_utility; }
  static void g_get_name(pfc::string_base& out) { out = "mpv"; }
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
      update_player_window();
    }

    if (p_what == ui_element_notify_colors_changed ||
        p_what == ui_element_notify_font_changed) {
      Invalidate();
    }

    if (p_what == ui_element_notify_edit_mode_changed) {
      if (p_param1 == 1) {
        SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) |
                                       WS_EX_TRANSPARENT | WS_EX_LAYERED);
      } else {
        SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) &
                                       ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));
      }
    }
  };

  enum {
    ID_PIN = 1003,
    ID_POPOUT = 1004,
  };

  void add_menu_items(CMenu* menu, CMenuDescriptionHybrid* menudesc) {
    menu->AppendMenu(container_is_pinned() ? MF_CHECKED : MF_UNCHECKED, ID_PIN,
                     _T("Pin here"));
    menudesc->Set(ID_PIN, "Pin the video to this container");

    if (container_is_on()) {
      menu->AppendMenu(MF_DEFAULT, ID_POPOUT, _T("Pop out"));
      menudesc->Set(ID_POPOUT, "Open video in popup");
    }
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
      default:
        break;
    }
  }

 private:
  ui_element_config::ptr m_config;

 protected:
  const ui_element_instance_callback_ptr m_callback;
};

class ui_element_mpvimpl : public ui_element_impl<CMpvDuiWindow> {};
static service_factory_single_t<ui_element_mpvimpl>
    g_ui_element_mpvimpl_factory;
}  // namespace mpv
