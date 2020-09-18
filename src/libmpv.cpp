#include "stdafx.h"
// PCH ^

#include <sstream>

#include "libmpv.h"

namespace mpv {
static libmpv_ dll;

libmpv_::libmpv_() : is_loaded(false){};

libmpv_* libmpv() {
  if (!dll.load_dll()) {
    throw exception_messagebox("Error loading libmpv-1.dll");
  }
  return &dll;
}

bool libmpv_::load_dll() {
  if (is_loaded) return true;

  HMODULE dll_module;
  pfc::string_formatter path = core_api::get_my_full_path();
  path.truncate(path.scan_filename());
  std::wstringstream wpath_mpv;
  wpath_mpv << path << "mpv\\mpv-1.dll";
  dll_module = LoadLibraryExW(wpath_mpv.str().c_str(), NULL,
                              LOAD_WITH_ALTERED_SEARCH_PATH);

  if (dll_module == NULL) {
    std::stringstream error;
    error << "mpv: Could not load mpv-1.dll: error code " << GetLastError();
    console::error(error.str().c_str());
    return false;
  }

  error_string =
      (mpv_error_string)GetProcAddress(dll_module, "mpv_error_string");
  free = (mpv_free)GetProcAddress(dll_module, "mpv_free");
  client_name =
      (mpv_client_name)GetProcAddress(dll_module, "mpv_client_name");
  client_id = (mpv_client_id)GetProcAddress(dll_module, "mpv_client_id");
  create = (mpv_create)GetProcAddress(dll_module, "mpv_create");
  initialize =
      (mpv_initialize)GetProcAddress(dll_module, "mpv_initialize");
  destroy = (mpv_destroy)GetProcAddress(dll_module, "mpv_destroy");
  terminate_destroy = (mpv_terminate_destroy)GetProcAddress(
      dll_module, "mpv_terminate_destroy");
  create_client =
      (mpv_create_client)GetProcAddress(dll_module, "mpv_create_client");
  create_weak_client = (mpv_create_weak_client)GetProcAddress(
      dll_module, "mpv_create_weak_client");
  load_config_file =
      (mpv_load_config_file)GetProcAddress(dll_module, "mpv_load_config_file");
  get_time_us =
      (mpv_get_time_us)GetProcAddress(dll_module, "mpv_get_time_us");
  free_node_contents = (mpv_free_node_contents)GetProcAddress(
      dll_module, "mpv_free_node_contents");
  set_option =
      (mpv_set_option)GetProcAddress(dll_module, "mpv_set_option");
  set_option_string = (mpv_set_option_string)GetProcAddress(
      dll_module, "mpv_set_option_string");
  command = (mpv_command)GetProcAddress(dll_module, "mpv_command");
  command_node =
      (mpv_command_node)GetProcAddress(dll_module, "mpv_command_node");
  command_ret =
      (mpv_command_ret)GetProcAddress(dll_module, "mpv_command_ret");
  command_string =
      (mpv_command_string)GetProcAddress(dll_module, "mpv_command_string");
  command_async =
      (mpv_command_async)GetProcAddress(dll_module, "mpv_command_async");
  command_node_async = (mpv_command_node_async)GetProcAddress(
      dll_module, "mpv_command_node_async");
  abort_async_command = (mpv_abort_async_command)GetProcAddress(
      dll_module, "mpv_abort_async_command");
  set_property =
      (mpv_set_property)GetProcAddress(dll_module, "mpv_set_property");
  set_property_string = (mpv_set_property_string)GetProcAddress(
      dll_module, "mpv_set_property_string");
  set_property_async = (mpv_set_property_async)GetProcAddress(
      dll_module, "mpv_set_property_async");
  get_property =
      (mpv_get_property)GetProcAddress(dll_module, "mpv_get_property");
  get_property_string = (mpv_get_property_string)GetProcAddress(
      dll_module, "mpv_get_property_string");
  get_property_osd_string = (mpv_get_property_osd_string)GetProcAddress(
      dll_module, "mpv_get_property_osd_string");
  get_property_async = (mpv_get_property_async)GetProcAddress(
      dll_module, "mpv_get_property_async");
  observe_property =
      (mpv_observe_property)GetProcAddress(dll_module, "mpv_observe_property");
  unobserve_property = (mpv_unobserve_property)GetProcAddress(
      dll_module, "mpv_unobserve_property");
  event_name =
      (mpv_event_name)GetProcAddress(dll_module, "mpv_event_name");
  event_to_node =
      (mpv_event_to_node)GetProcAddress(dll_module, "mpv_event_to_node");
  request_event =
      (mpv_request_event)GetProcAddress(dll_module, "mpv_request_event");
  request_log_messages = (mpv_request_log_messages)GetProcAddress(
      dll_module, "mpv_request_log_messages");
  wait_event =
      (mpv_wait_event)GetProcAddress(dll_module, "mpv_wait_event");
  wakeup = (mpv_wakeup)GetProcAddress(dll_module, "mpv_wakeup");
  set_wakeup_callback = (mpv_set_wakeup_callback)GetProcAddress(
      dll_module, "mpv_set_wakeup_callback");
  wait_async_requests = (mpv_wait_async_requests)GetProcAddress(
      dll_module, "mpv_wait_async_requests");
  hook_add = (mpv_hook_add)GetProcAddress(dll_module, "mpv_hook_add");
  hook_continue =
      (mpv_hook_continue)GetProcAddress(dll_module, "mpv_hook_continue");

  is_loaded = true;
  return true;
}
}  // namespace libmpv
