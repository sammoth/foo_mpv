#include "stdafx.h"
// PCH ^

#include "menu_utils.h"

namespace menu_utils {
static class string8Hasher {
 public:
  std::size_t operator()(const pfc::string8& key) const {
    t_uint64 hash = hasher_md5::get()
                        ->process_single(key.get_ptr(), key.get_length())
                        .xorHalve();
    size_t ret = hash >> 8*(sizeof(t_uint64)-sizeof(size_t));
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

menu_entry get_mainmenu_item(pfc::string8 name) {
  auto stored = menu_memo.find(name);
  if (stored != menu_memo.end()) {
    return stored->second;
  }

  for (auto& item : get_mainmenu_items()) {
    if (item.name && item.name.equals(name)) {
      menu_memo[name] = item;
      return item;
    }
  }
  return {pfc::guid_null, pfc::guid_null, pfc::string8()};
}
menu_entry get_contextmenu_item(pfc::string8 name) {
  auto stored = context_memo.find(name);
  if (stored != context_memo.end()) {
    return stored->second;
  }

  for (auto& item : get_contextmenu_items()) {
    if (item.name && item.name.equals(name)) {
      context_memo[name] = item;
      return item;
    }
  }
  return {pfc::guid_null, pfc::guid_null, pfc::string8()};
}
}  // namespace menu_utils
