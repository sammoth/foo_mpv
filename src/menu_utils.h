#include "stdafx.h"
// PCH ^
#pragma once
#include "../columns_ui-sdk/ui_extension.h"

namespace menu_utils {
// running menu items
struct menu_entry {
  GUID guid;
  GUID subcommand;
  pfc::string8 name;
};
bool run_mainmenu_item(pfc::string8 name);
bool run_contextmenu_item(pfc::string8 name, metadb_handle_list_cref items);
std::vector<menu_entry> get_contextmenu_items();
std::vector<menu_entry> get_mainmenu_items();

// building menus
class menu_node_disabled : public uie::menu_node_command_t {
  pfc::string8 m_text;

 public:
  menu_node_disabled(pfc::string8 text);
  void execute();
  bool get_description(pfc::string_base& p_out) const;
  bool get_display_data(pfc::string_base& p_out,
                        unsigned& p_displayflags) const;
};

class menu_node_run : public uie::menu_node_command_t {
  pfc::string8 m_text;
  pfc::string8 m_description;
  std::function<void()> m_executor;
  bool m_checked;

 public:
  menu_node_run(pfc::string8 text, pfc::string8 description, bool checked,
                std::function<void()> execute);
  menu_node_run(pfc::string8 text, bool checked, std::function<void()> execute);
  void execute();
  bool get_description(pfc::string_base& p_out) const;
  bool get_display_data(pfc::string_base& p_out,
                        unsigned& p_displayflags) const;
};

class menu_node_popup : public uie::menu_node_popup_t {
  pfc::string8 m_text;
  pfc::string8 m_description;
  std::vector<ui_extension::menu_node_ptr> m_items;

 public:
  menu_node_popup(pfc::string8 text, pfc::string8 description,
                  std::vector<ui_extension::menu_node_ptr> children);
  menu_node_popup(pfc::string8 text,
                  std::vector<ui_extension::menu_node_ptr> children);

  void get_child(unsigned p_index, uie::menu_node_ptr& p_out) const;
  unsigned get_children_count() const;
  bool get_description(pfc::string_base& p_out) const;
  bool get_display_data(pfc::string_base& p_out,
                        unsigned& p_displayflags) const;
};
}  // namespace menu_utils
