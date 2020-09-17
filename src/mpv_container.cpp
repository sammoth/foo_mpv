#include "stdafx.h"
// PCH ^

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

mpv_container* get_main_container() {
  double top_priority = -1.0;
  mpv_container* main = NULL;
  for (auto it = g_mpv_containers.begin(); it != g_mpv_containers.end(); ++it) {
    if ((*it)->container_is_pinned() ||
        ((*it)->is_popup() && (main == NULL || !main->container_is_pinned()))) {
      main = (*it);
      break;
    }
    if (!(*it)->is_visible()) continue;

    double priority = 0;
    switch (cfg_panel_metric) {
      case 0:
        priority = (*it)->cx * (*it)->cy;
        break;
      case 1:
        priority = (*it)->cx;
        break;
      case 2:
        priority = (*it)->cy;
        break;
    }
    if (priority > top_priority) {
      main = *it;
      top_priority = priority;
      continue;
    }
  }

  if (main == NULL) {
    main = *g_mpv_containers.begin();
  }
  return main;
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

      if (g_mpv_player) {
        g_mpv_player->handle_menu_cmd(cmd);
      }
      handle_menu_cmd(cmd);
    }
  } catch (std::exception const& e) {
    console::complain("Context menu failure", e);
  }
}

void mpv_container::container_toggle_fullscreen() {
  if (g_mpv_player) {
    g_mpv_player->toggle_fullscreen();
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
