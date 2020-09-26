#include "stdafx.h"
// PCH ^

#include <algorithm>

#include "mpv_container.h"
#include "mpv_player.h"
#include "preferences.h"

namespace mpv {
extern cfg_uint cfg_bg_color, cfg_panel_metric;

static std::vector<mpv_container*> g_mpv_containers;
static CWindowAutoLifetime<mpv_player>* g_mpv_player;

void invalidate_all_containers() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).invalidate();
  }
}

void mpv_on_new_artwork() {
  if (g_mpv_player != NULL) {
      g_mpv_player->on_new_artwork();
  }
}

bool container_compare(mpv_container* a, mpv_container* b) {
  // true if a goes before b
  if (a->is_fullscreen()) return true;
  if (b->is_fullscreen()) return false;
  if (a->container_is_pinned()) return true;
  if (b->container_is_pinned()) return false;
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

mpv_container* get_main_container() {
  if (g_mpv_containers.empty()) return NULL;
  mpv_container* main = NULL;
  std::sort(g_mpv_containers.begin(), g_mpv_containers.end(),
            container_compare);
  if ((*g_mpv_containers.begin())->container_is_pinned()) {
    for (auto it = g_mpv_containers.begin() + 1; it != g_mpv_containers.end();
         ++it) {
      (*it)->notify_pinned_elsewhere();
    }
  }
  return *g_mpv_containers.begin();
}

bool mpv_container::container_is_on() {
  return g_mpv_player != NULL && g_mpv_player->contained_in(this);
}

void mpv_container::update_player_window() {
  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

bool mpv_container::container_is_pinned() { return pinned; }

t_ui_color mpv_container::get_bg() { return cfg_bg_color; }

void mpv_container::container_unpin() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

void mpv_container::container_pin() {
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    (**it).pinned = false;
  }

  pinned = true;

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}

void mpv_container::container_on_context_menu(CWindow wnd, CPoint point) {
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

      if (g_mpv_player) {
        g_mpv_player->add_menu_items(&menu, &menudesc);
      }

      add_menu_items(&menu, &menudesc);

      int cmd =
          menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                              point.x, point.y, menudesc, 0);

      handle_menu_cmd(cmd);

      if (g_mpv_player) {
        g_mpv_player->handle_menu_cmd(cmd);
      }
    }
  } catch (std::exception const& e) {
    console::complain("Context menu failure", e);
  }
}

void mpv_container::container_resize(long p_x, long p_y) {
  cx = p_x;
  cy = p_y;
  update_player_window();
}

void mpv_container::container_create() {
  g_mpv_containers.push_back(this);

  if (!g_mpv_player) {
    g_mpv_player = new CWindowAutoLifetime<mpv_player>(this->container_wnd());
  } else {
    g_mpv_player->update();
  }
}

void mpv_container::container_destroy() {
  g_mpv_containers.erase(
      std::remove(g_mpv_containers.begin(), g_mpv_containers.end(), this),
      g_mpv_containers.end());

  if (g_mpv_containers.empty()) {
    if (g_mpv_player != NULL) g_mpv_player->destroy();
    g_mpv_player = NULL;
  }

  if (g_mpv_player) {
    g_mpv_player->update();
  }
}
}  // namespace mpv
