#include "stdafx.h"
// PCH ^

#include <../helpers/atl-misc.h>

#include <sstream>

#include "menu_utils.h"
#include "player_container.h"
#include "player.h"
#include "popup_window.h"
#include "fullscreen_window.h"
#include "preferences.h"
#include "resource.h"

namespace mpv {
static popup_window* g_popup;

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


DWORD popup_window::GetWndStyle(DWORD style) {
  return WS_OVERLAPPEDWINDOW | WS_VISIBLE;
}

DWORD popup_window::GetWndExStyle(DWORD dwExStyle) {
  if (cfg_mpv_popup_alwaysontop) {
    return WS_EX_TOPMOST;
  } else {
    return 0;
  }
};

BOOL popup_window::on_erase_bg(CDCHandle dc) {
  CRect rc;
  WIN32_OP_D(GetClientRect(&rc));
  CBrush brush;
  WIN32_OP_D(brush.CreateSolidBrush(get_bg()) != NULL);
  WIN32_OP_D(dc.FillRect(&rc, brush));

  return TRUE;
}

void popup_window::on_keydown(UINT wp, UINT l, UINT h) {
  switch (wp) {
    case VK_ESCAPE:
      DestroyWindow();
    default:
      mpv::player::send_message(WM_KEYDOWN, wp, MAKELPARAM(l, h));
      break;
  }
}

void popup_window::on_keyup(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_KEYUP, wp, MAKELPARAM(l, h));
}

void popup_window::on_syskeydown(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_SYSKEYDOWN, wp, MAKELPARAM(l, h));
}

void popup_window::on_syskeyup(UINT wp, UINT l, UINT h) {
  mpv::player::send_message(WM_SYSKEYUP, wp, MAKELPARAM(l, h));
}

void popup_window::toggle_fullscreen() {
  MONITORINFO monitor;
  monitor.cbSize = sizeof(monitor);
  GetMonitorInfoW(MonitorFromWindow(container_wnd(), MONITOR_DEFAULTTONEAREST),
                  &monitor);

  fullscreen_window::open(true, monitor);
};

void popup_window::on_lose_player() { DestroyWindow(); }

bool popup_window::is_osc_enabled() { return true; }

LRESULT popup_window::on_create(LPCREATESTRUCT st) {
  if (cfg_mpv_popup_separate) {
    SetWindowLongPtr(GWLP_HWNDPARENT, NULL);
  } else {
    SetWindowLongPtr(GWLP_HWNDPARENT, (LONG_PTR)core_api::get_main_window());
  }

  SetClassLong(get_wnd(), GCL_HICON, (LONG)ui_control::get()->get_main_icon());

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
                 monitor_info.rcMonitor.top + height / 4, width / 2, height / 2,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
  }

  g_popup = this;
  player_container::on_create();

  unpin();

  return 0;
}

void popup_window::on_destroy() {
  RECT client_rect = {};
  GetWindowRect(&client_rect);
  cfg_mpv_popup_rect = client_rect;
  player_container::on_destroy();
  g_popup = NULL;
}

void popup_window::on_size(UINT wparam, CSize size) {
  player_container::on_resize(size.cx, size.cy);
}

void popup_window::add_menu_items(uie::menu_hook_impl& menu_hook) {
  if (menu_hook.get_children_count() > 0) {
    menu_hook.add_node(new uie::menu_node_separator_t());
  }
  menu_hook.add_node(new menu_utils::menu_node_run(
      "Always on-top", "Keep the video window above other windows",
      cfg_mpv_popup_alwaysontop, [this]() {
        cfg_mpv_popup_alwaysontop = !cfg_mpv_popup_alwaysontop;
        fb2k::inMainThread([this]() {
          close();
          open();
        });
      }));
  menu_hook.add_node(new menu_utils::menu_node_run(
      "Separate from main window",
      "Allow window to separate from the foobar2000 main window",
      cfg_mpv_popup_separate, [this]() {
        cfg_mpv_popup_separate = !cfg_mpv_popup_separate;
        fb2k::inMainThread([this]() {
          close();
          open();
        });
      }));
}

void popup_window::open() { open(true); }

void popup_window::open(bool pop_existing) {
  if (g_popup) {
    if (pop_existing) {
      g_popup->BringWindowToTop();
    }
    return;
  }

  try {
    new CWindowAutoLifetime<popup_window>(NULL);
  } catch (std::exception const& e) {
    popup_message::g_complain("Popup creation failure", e);
  }
}

void popup_window::close() {
  if (g_popup) {
    g_popup->DestroyWindow();
    g_popup = NULL;
  }
}

void popup_window::set_title(pfc::string8 title) {
  if (g_popup) {
    uSetWindowText(g_popup->m_hWnd, title);
  }
}

class close_popup_initquit : public initquit {
 public:
  void on_quit() override { popup_window::close(); }
};

static initquit_factory_t<close_popup_initquit> g_popup_closer;
}  // namespace mpv
