#include "stdafx.h"
// PCH ^

#include <algorithm>

#include "columns_ui-sdk/ui_extension.h"
#include "player.h"
#include "player_container.h"
#include "fullscreen_window.h"
#include "preferences.h"

namespace mpv {
extern cfg_uint cfg_bg_color, cfg_panel_metric;

static std::vector<player_container*> g_containers;
static player_container* pinned_container;

void player_container::invalidate_all_containers() {
  for (auto& c : g_containers) {
    c->invalidate();
  }
}

bool player_container::owns_player() {
  return player_container::get_main_container() == this;
}

static bool container_compare(player_container* a, player_container* b) {
  if (a->is_fullscreen()) return true;
  if (b->is_fullscreen()) return false;
  if (a->is_pinned()) return true;
  if (b->is_pinned()) return false;
  if (a->is_popup()) return true;
  if (b->is_popup()) return false;
  if (!a->is_visible()) return false;
  if (!b->is_visible()) return true;

  switch (cfg_panel_metric) {
    case 0:
      return (a->cx * a->cy > b->cx * b->cy);
    case 1:
      return (a->cx > b->cx);
    case 2:
      return (a->cy > b->cy);
    default:
      uBugCheck();
  }
}

player_container* player_container::get_main_container() {
  if (g_containers.empty()) return NULL;
  player_container* main = NULL;
  std::sort(g_containers.begin(), g_containers.end(), container_compare);
  return *g_containers.begin();
}

bool player_container::is_pinned() { return pinned_container == this; }

t_ui_color player_container::get_bg() { return cfg_bg_color; }

void player_container::unpin() {
  pinned_container = NULL;
  player::on_containers_change();
}

void player_container::pin() {
  pinned_container = this;
  player::on_containers_change();
}

void player_container::on_context_menu(CWindow wnd, CPoint pt) {
  pfc::refcounted_object_ptr_t<ui_extension::menu_hook_impl> menu_hook =
      new ui_extension::menu_hook_impl;
  if (owns_player()) {
    player::add_menu_items(*menu_hook);
  }

  add_menu_items(*menu_hook);

  HMENU menu = CreatePopupMenu();
  menu_hook->win32_build_menu(menu, 1, pfc_infinite);
  menu_helpers::win32_auto_mnemonics(menu);
  const auto cmd = static_cast<unsigned>(
      TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x,
                     pt.y, 0, container_wnd(), nullptr));
  if (cmd >= 1) {
    menu_hook->execute_by_id(cmd);
  }

  DestroyMenu(menu);
}

void player_container::on_resize(long p_x, long p_y) {
  cx = p_x;
  cy = p_y;
  player::on_containers_change();
}

void player_container::on_create() {
  g_containers.push_back(this);
  player::on_containers_change();
}

void player_container::on_destroy() {
  g_containers.erase(
      std::remove(g_containers.begin(), g_containers.end(), this),
      g_containers.end());
  if (pinned_container == this) pinned_container = NULL;
  player::on_containers_change();
}

void player_container::toggle_fullscreen() {
  MONITORINFO monitor;
  monitor.cbSize = sizeof(monitor);
  GetMonitorInfoW(MonitorFromWindow(container_wnd(), MONITOR_DEFAULTTONEAREST),
                  &monitor);

  fullscreen_window::open(false, monitor);
};

}  // namespace mpv
