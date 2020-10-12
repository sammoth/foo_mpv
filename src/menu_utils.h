#include "stdafx.h"
// PCH ^
#pragma once

namespace menu_utils {
struct menu_entry {
  GUID guid;
  GUID subcommand;
  pfc::string8 name;
};
bool run_mainmenu_item(pfc::string8 name);
bool run_contextmenu_item(pfc::string8 name, metadb_handle_list_cref items);
std::vector<menu_entry> get_contextmenu_items();
std::vector<menu_entry> get_mainmenu_items();
}  // namespace menu_helper
