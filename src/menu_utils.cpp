#include "stdafx.h"
// PCH ^

#include "columns_ui-sdk/ui_extension.h"
#include "menu_utils.h"

namespace menu_utils {
menu_node_disabled::menu_node_disabled(pfc::string8 text) : m_text(text){};
void menu_node_disabled::execute() {}
bool menu_node_disabled::get_description(pfc::string_base& p_out) const {
  return false;
}
bool menu_node_disabled::get_display_data(pfc::string_base& p_out,
                                          unsigned& p_displayflags) const {
  p_out = m_text;
  p_displayflags = state_disabled;
  return true;
}

menu_node_run::menu_node_run(pfc::string8 text, pfc::string8 description,
                             bool checked, std::function<void()> execute)
    : m_text(text),
      m_description(description),
      m_executor(execute),
      m_checked(checked){};
menu_node_run::menu_node_run(pfc::string8 text, bool checked,
                             std::function<void()> execute)
    : m_text(text), m_description(), m_executor(execute), m_checked(checked){};
void menu_node_run::execute() { m_executor(); }
bool menu_node_run::get_description(pfc::string_base& p_out) const {
  if (m_description.is_empty()) {
    return false;
  } else {
    p_out = m_description;
    return true;
  }
}
bool menu_node_run::get_display_data(pfc::string_base& p_out,
                                     unsigned& p_displayflags) const {
  p_out = m_text;
  p_displayflags = m_checked ? state_checked : 0;
  return true;
}

menu_node_popup::menu_node_popup(pfc::string8 text, std::vector<ui_extension::menu_node_ptr> children)
    : m_text(text), m_description(), m_items(children){};
menu_node_popup::menu_node_popup(pfc::string8 text, pfc::string8 description, std::vector<ui_extension::menu_node_ptr> children)
    : m_text(text), m_description(description), m_items(children){};
void menu_node_popup::get_child(unsigned p_index,
                                uie::menu_node_ptr& p_out) const {
  p_out = m_items[p_index].get_ptr();
}
unsigned menu_node_popup::get_children_count() const { return m_items.size(); }
bool menu_node_popup::get_description(pfc::string_base& p_out) const {
  if (m_description.is_empty()) {
    return false;
  } else {
    p_out = m_description;
    return true;
  }
}
bool menu_node_popup::get_display_data(pfc::string_base& p_out,
                                       unsigned& p_displayflags) const {
  p_out = m_text;
  p_displayflags = 0;
  return true;
}

class string8Hasher {
 public:
  std::size_t operator()(const pfc::string8& key) const {
    t_uint64 hash = hasher_md5::get()
                        ->process_single(key.get_ptr(), key.get_length())
                        .xorHalve();
    size_t ret = hash >> 8 * (sizeof(t_uint64) - sizeof(size_t));
    return ret;
  }
};
std::unordered_map<pfc::string8, menu_entry, string8Hasher> context_memo;
std::unordered_map<pfc::string8, menu_entry, string8Hasher> menu_memo;

static bool get_context_items_r(GUID guid, std::vector<menu_entry>& things,
                                std::list<pfc::string8> name_parts,
                                contextmenu_item_node* p_node, bool b_root) {
  if (p_node) {
    pfc::string8 name;
    unsigned _;
    p_node->get_display_data(name, _, metadb_handle_list(),
                             contextmenu_item::caller_keyboard_shortcut_list);
    if (!name.is_empty()) name_parts.emplace_back(name);

    if (p_node->get_type() == contextmenu_item_node::TYPE_POPUP) {
      const unsigned child_count = p_node->get_children_count();

      for (unsigned child = 0; child < child_count; child++) {
        contextmenu_item_node* p_child = p_node->get_child(child);
        get_context_items_r(guid, things, name_parts, p_child, false);
      }
      return true;
    }
    if (p_node->get_type() == contextmenu_item_node::TYPE_COMMAND && !b_root) {
      pfc::string8 item;
      for (auto& n : name_parts) {
        item << "/" << n;
      }
      menu_entry thing = {guid, p_node->get_guid(), item};
      things.emplace_back(thing);
      return true;
    }
  }

  return false;
}

static bool get_mainmenu_dynamic_r(GUID guid, std::vector<menu_entry>& things,
                                   const mainmenu_node::ptr& ptr_node,
                                   std::list<pfc::string8> name_parts,
                                   bool b_root) {
  if (ptr_node.is_valid()) {
    pfc::string8 name_part;
    t_uint32 flags;
    ptr_node->get_display(name_part, flags);

    switch (ptr_node->get_type()) {
      case mainmenu_node::type_command: {
        name_parts.emplace_back(name_part);

        pfc::string8 item;
        for (auto& n : name_parts) {
          item << "/" << n;
        }
        menu_entry thing = {guid, ptr_node->get_guid(), item};
        things.emplace_back(thing);
      }
        return true;
      case mainmenu_node::type_group: {
        if (!b_root) name_parts.emplace_back(name_part);

        for (t_size i = 0, count = ptr_node->get_children_count(); i < count;
             i++) {
          mainmenu_node::ptr ptr_child = ptr_node->get_child(i);
          get_mainmenu_dynamic_r(guid, things, ptr_child, name_parts, false);
        }
      }
        return true;
      default:
        return false;
    }
  }
  return false;
}

std::vector<menu_entry> get_contextmenu_items() {
  std::vector<menu_entry> ret;

  service_enum_t<contextmenu_item> e;
  service_ptr_t<contextmenu_item> ptr;
  while (e.next(ptr)) {
    for (unsigned i = 0; i < ptr->get_num_items(); i++) {
      pfc::ptrholder_t<contextmenu_item_node_root> p_node(ptr->instantiate_item(
          i, metadb_handle_list(),
          contextmenu_item::caller_keyboard_shortcut_list));

      std::list<pfc::string8> name_parts;
      GUID parent = ptr->get_parent_();
      contextmenu_group::ptr group;
      while (parent != contextmenu_groups::root) {
        for (auto e = FB2K_ENUMERATE(contextmenu_group); !e.finished(); ++e) {
          auto srv = *e;
          if (srv->get_guid() == parent) {
            group = srv;
            contextmenu_group_popup::ptr popup;
            if (group->service_query_t(popup)) {
              pfc::string8 group_name;
              popup->get_name(group_name);
              name_parts.emplace_front(group_name);
            }
            parent = group->get_parent();
            break;
          }
        }
      }

      if (p_node.is_valid() &&
          get_context_items_r(ptr->get_item_guid(i), ret, name_parts,
                              p_node.get_ptr(), true)) {
      } else {
        pfc::string8 name;
        ptr->get_item_name(i, name);
        name_parts.emplace_back(name);

        pfc::string8 row;
        for (auto& n : name_parts) {
          row << "/" << n;
        }
        menu_entry thing = {ptr->get_item_guid(i), pfc::guid_null, row};
        ret.emplace_back(thing);
      }
    }
  }

  return ret;
}

std::vector<menu_entry> get_mainmenu_items() {
  std::vector<menu_entry> ret;

  service_enum_t<mainmenu_commands> e;
  service_ptr_t<mainmenu_commands> ptr;
  while (e.next(ptr)) {
    service_ptr_t<mainmenu_commands_v2> ptr_v2;
    ptr->service_query_t(ptr_v2);
    for (unsigned i = 0; i < ptr->get_command_count(); i++) {
      pfc::string8 name;
      ptr->get_name(i, name);

      std::list<pfc::string8> name_parts;
      name_parts.emplace_back(name);

      GUID parent = ptr->get_parent();
      mainmenu_group::ptr group;
      while (parent != pfc::guid_null) {
        pfc::string8 parentname;

        for (auto e = FB2K_ENUMERATE(mainmenu_group); !e.finished(); ++e) {
          auto srv = *e;
          if (srv->get_guid() == parent) {
            group = srv;
            mainmenu_group_popup::ptr popup;
            if (group->service_query_t(popup)) {
              pfc::string8 group_name;
              popup->get_name(group_name);
              name_parts.emplace_front(group_name);
            }
            parent = group->get_parent();
            break;
          }
        }
      }

      if (ptr_v2.is_valid() && ptr_v2->is_command_dynamic(i)) {
        mainmenu_node::ptr ptr_node = ptr_v2->dynamic_instantiate(i);
        get_mainmenu_dynamic_r(ptr->get_command(i), ret, ptr_node, name_parts,
                               true);
      } else {
        pfc::string8 row;
        for (auto& n : name_parts) {
          row << "/" << n;
        }
        menu_entry thing = {ptr->get_command(i), pfc::guid_null, row};
        ret.emplace_back(thing);
      }
    }
  }

  return ret;
}

bool run_mainmenu_item(pfc::string8 name) {
  auto entry = menu_memo.find(name);
  bool success = false;
  if (entry != menu_memo.end()) {
    if (entry->second.subcommand != pfc::guid_null) {
      success = mainmenu_commands::g_execute_dynamic(entry->second.guid,
                                                     entry->second.subcommand);
    } else {
      success = mainmenu_commands::g_execute(entry->second.guid);
    }
  }

  if (!success) {
    menu_memo.erase(name);
    for (auto& item : get_mainmenu_items()) {
      if (item.name && item.name.equals(name)) {
        if (item.subcommand != pfc::guid_null) {
          success =
              mainmenu_commands::g_execute_dynamic(item.guid, item.subcommand);
        } else {
          success = mainmenu_commands::g_execute(item.guid);
        }

        if (success) {
          menu_memo[name] = item;
        }
        break;
      }
    }
  }

  if (!success) {
    FB2K_console_formatter() << "mpv: Failure running menu command " << name;
  }

  return success;
}

bool run_contextmenu_item(pfc::string8 name, metadb_handle_list_cref items) {
  auto entry = context_memo.find(name);
  bool success = false;
  if (entry != context_memo.end()) {
    success = menu_helpers::run_command_context(
        entry->second.guid, entry->second.subcommand, items);
  }

  if (!success) {
    context_memo.erase(name);
    for (auto& item : get_contextmenu_items()) {
      if (item.name && item.name.equals(name)) {
        success = menu_helpers::run_command_context(item.guid, item.subcommand,
                                                    items);
        if (success) {
          context_memo[name] = item;
        }
        break;
      }
    }
  }

  if (!success) {
    FB2K_console_formatter() << "mpv: Failure running context command " << name;
  }

  return success;
}
}  // namespace menu_utils
