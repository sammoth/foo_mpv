#include "stdafx.h"
// PCH ^

#include <algorithm>

#include "mpv_container.h"
#include "mpv_player.h"
#include "preferences.h"

namespace mpv {
extern cfg_uint cfg_bg_color, cfg_panel_metric;

static std::vector<mpv_container*> g_containers;
static mpv_container* pinned_container;

void mpv_container::invalidate_all_containers() {
  for (auto& c : g_containers) {
    c->invalidate();
  }
}

bool mpv_container::owns_player() {
  return mpv_container::get_main_container() == this;
}

static bool container_compare(mpv_container* a, mpv_container* b) {
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

mpv_container* mpv_container::get_main_container() {
  if (g_containers.empty()) return NULL;
  mpv_container* main = NULL;
  std::sort(g_containers.begin(), g_containers.end(), container_compare);
  return *g_containers.begin();
}

bool mpv_container::is_pinned() { return pinned_container == this; }

t_ui_color mpv_container::get_bg() { return cfg_bg_color; }

void mpv_container::unpin() {
  pinned_container = NULL;
  mpv_player::on_containers_change();
}

void mpv_container::pin() {
  pinned_container = this;
  mpv_player::on_containers_change();
}

void mpv_container::on_context_menu(CWindow wnd, CPoint point) {
  try {
    {
      // handle the context menu key case - center the menu
      if (point == CPoint(-1, -1)) {
        CRect rc;
        WIN32_OP(wnd.GetWindowRect(&rc));
        point = rc.CenterPoint();
      }

      CMenuDescriptionHybrid menudesc(container_wnd());

      static_api_ptr_t<contextmenu_manager> api;
      CMenu menu;
      WIN32_OP(menu.CreatePopupMenu());

      if (owns_player()) {
        mpv_player::add_menu_items(&menu, &menudesc);
      }

      add_menu_items(&menu, &menudesc);

      int cmd =
          menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                              point.x, point.y, menudesc, 0);

      handle_menu_cmd(cmd);

      mpv_player::handle_menu_cmd(cmd);
    }
  } catch (std::exception const& e) {
    console::complain("Context menu failure", e);
  }
}

void mpv_container::on_resize(long p_x, long p_y) {
  cx = p_x;
  cy = p_y;
  mpv_player::on_containers_change();
}

void mpv_container::on_create() {
  g_containers.push_back(this);
  mpv_player::on_containers_change();
}

void mpv_container::on_destroy() {
  g_containers.erase(
      std::remove(g_containers.begin(), g_containers.end(), this),
      g_containers.end());
  if (pinned_container == this) pinned_container = NULL;
  mpv_player::on_containers_change();
}
}  // namespace mpv
