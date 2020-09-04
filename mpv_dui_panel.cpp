#include "stdafx.h"
// PCH ^

#include <helpers/BumpableElem.h>

#include <sstream>

#include "libmpv.h"

namespace mpv {
static const GUID guid_mpv_dui_panel = {
    0x777a523a, 0x1ed, 0x48b9, {0xb9, 0x1, 0xda, 0xb1, 0xbe, 0x31, 0x7c, 0xa4}};

struct CMpvDuiWindow : public ui_element_instance, CWindowImpl<CMpvDuiWindow> {
 public:
  DECLARE_WND_CLASS_EX(TEXT("{9D6179F4-0A94-4F76-B7EB-C4A853853DCB}"),
                       CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, (-1));

  BEGIN_MSG_MAP_EX(CMpvDuiWindow)
  MSG_WM_ERASEBKGND(on_erase_bg)
  MSG_WM_SIZE(on_size)
  MSG_WM_DESTROY(on_destroy)
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

  void on_destroy() {
    if (player_window != NULL) player_window->DestroyWindow();
  }

  void on_size(UINT wparam, CSize size) {
    if (player_window != NULL) player_window->MaybeResize(size.cx, size.cy);
  }

  void initialize_window(HWND parent) {
    HWND wid;
    WIN32_OP(wid = Create(parent, 0, 0, WS_CHILD, 0));

    player_window = std::unique_ptr<CMpvWindow>(new CMpvWindow(*this));

    if (m_callback->is_elem_visible_(this)) {
      player_window->mpv_enable();
    }
  }

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
      if (p_param1 == 1) {
        player_window->mpv_enable();
      } else {
        player_window->mpv_disable();
      }
    }
  };

 private:
  ui_element_config::ptr m_config;
  std::unique_ptr<CMpvWindow> player_window;

 protected:
  const ui_element_instance_callback_ptr m_callback;
};

class ui_element_mpvimpl : public ui_element_impl_withpopup<CMpvDuiWindow> {};
static service_factory_single_t<ui_element_mpvimpl>
    g_ui_element_mpvimpl_factory;
}  // namespace mpv
