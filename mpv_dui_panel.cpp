#include "stdafx.h"
// PCH ^

#include <helpers/BumpableElem.h>

#include <sstream>

#include "libmpv.h"
#include "resource.h"

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
      : m_callback(p_callback), m_config(config) {}

  BOOL on_erase_bg(CDCHandle dc) {
    CRect rc;
    WIN32_OP_D(GetClientRect(&rc));
    CBrush brush;
    WIN32_OP_D(brush.CreateSolidBrush(0x00000000) != NULL);
    WIN32_OP_D(dc.FillRect(&rc, brush));
    return TRUE;
  }

  void initialize_window(HWND parent) {
    WIN32_OP(Create(parent, 0, 0, WS_CHILD, 0));
    create();
  }

  void on_destroy() { destroy(); }

  void on_size(UINT wparam, CSize size) { resize(size.cx, size.cy); }

  double priority() override { return x * y; }

  void on_fullscreen(bool fullscreen) override {}

  HWND container_wnd() override { return m_hWnd; }

  bool is_visible() override { return m_callback->is_elem_visible_(this); }

  HWND get_wnd() { return m_hWnd; }

  void set_configuration(ui_element_config::ptr config) { m_config = config; }
  ui_element_config::ptr get_configuration() { return m_config; }
  static GUID g_get_guid() { return guid_mpv_dui_panel; }
  static GUID g_get_subclass() { return ui_element_subclass_utility; }
  static void g_get_name(pfc::string_base& out) { out = "mpv Video"; }
  static ui_element_config::ptr g_get_default_configuration() {
    return ui_element_config::g_create_empty(g_get_guid());
  }
  static const char* g_get_description() { return "mpv Video"; }
  void notify(const GUID& p_what, t_size p_param1, const void* p_param2,
              t_size p_param2size) {
    if (p_what == ui_element_notify_visibility_changed) {
      update();
    }
  };

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
        enum {
          ID_PIN = 4,
          ID_SETCOLOUR = 5,
          ID_CM_BASE,
        };
      menu.AppendMenu(is_pinned() ? MF_CHECKED : MF_UNCHECKED,
                      ID_PIN, _T("Pin here"));
      menudesc.Set(ID_PIN, "Pin the video to this container");

        int cmd =
            menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                                point.x, point.y, menudesc, 0);

        if (cmd > 0) {
          if (cmd >= ID_CM_BASE) {
            api->execute_by_id(cmd - ID_CM_BASE);
          } else
            switch (cmd) {
              case ID_PIN:
                if (is_pinned()) {
                  unpin();
                } else {
                  pin();
                }
                break;
            }
        }
      }
    } catch (std::exception const& e) {
      console::complain("Context menu failure", e);  // rare
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
