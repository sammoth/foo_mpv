#include "stdafx.h"
// PCH ^

#include <sstream>

#include "libmpv.h"

namespace libmpv {
static function_table functions = {};
function_table* get() { return &functions; }

class libmpv_loader : public initquit {
 public:
  void on_init() override {
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
      return;
    }

    functions.error_string =
        (mpv_error_string)GetProcAddress(dll_module, "mpv_error_string");
    functions.free = (mpv_free)GetProcAddress(dll_module, "mpv_free");
    functions.client_name =
        (mpv_client_name)GetProcAddress(dll_module, "mpv_client_name");
    functions.client_id = (mpv_client_id)GetProcAddress(dll_module, "mpv_client_id");
    functions.create = (mpv_create)GetProcAddress(dll_module, "mpv_create");
    functions.initialize =
        (mpv_initialize)GetProcAddress(dll_module, "mpv_initialize");
    functions.destroy = (mpv_destroy)GetProcAddress(dll_module, "mpv_destroy");
    functions.terminate_destroy = (mpv_terminate_destroy)GetProcAddress(
        dll_module, "mpv_terminate_destroy");
    functions.create_client =
        (mpv_create_client)GetProcAddress(dll_module, "mpv_create_client");
    functions.create_weak_client = (mpv_create_weak_client)GetProcAddress(
        dll_module, "mpv_create_weak_client");
    functions.load_config_file = (mpv_load_config_file)GetProcAddress(
        dll_module, "mpv_load_config_file");
    functions.get_time_us =
        (mpv_get_time_us)GetProcAddress(dll_module, "mpv_get_time_us");
    functions.free_node_contents = (mpv_free_node_contents)GetProcAddress(
        dll_module, "mpv_free_node_contents");
    functions.set_option =
        (mpv_set_option)GetProcAddress(dll_module, "mpv_set_option");
    functions.set_option_string = (mpv_set_option_string)GetProcAddress(
        dll_module, "mpv_set_option_string");
    functions.command = (mpv_command)GetProcAddress(dll_module, "mpv_command");
    functions.command_node =
        (mpv_command_node)GetProcAddress(dll_module, "mpv_command_node");
    functions.command_ret =
        (mpv_command_ret)GetProcAddress(dll_module, "mpv_command_ret");
    functions.command_string =
        (mpv_command_string)GetProcAddress(dll_module, "mpv_command_string");
    functions.command_async =
        (mpv_command_async)GetProcAddress(dll_module, "mpv_command_async");
    functions.command_node_async = (mpv_command_node_async)GetProcAddress(
        dll_module, "mpv_command_node_async");
    functions.abort_async_command = (mpv_abort_async_command)GetProcAddress(
        dll_module, "mpv_abort_async_command");
    functions.set_property =
        (mpv_set_property)GetProcAddress(dll_module, "mpv_set_property");
    functions.set_property_string = (mpv_set_property_string)GetProcAddress(
        dll_module, "mpv_set_property_string");
    functions.set_property_async = (mpv_set_property_async)GetProcAddress(
        dll_module, "mpv_set_property_async");
    functions.get_property =
        (mpv_get_property)GetProcAddress(dll_module, "mpv_get_property");
    functions.get_property_string = (mpv_get_property_string)GetProcAddress(
        dll_module, "mpv_get_property_string");
    functions.get_property_osd_string = (mpv_get_property_osd_string)GetProcAddress(
        dll_module, "mpv_get_property_osd_string");
    functions.get_property_async = (mpv_get_property_async)GetProcAddress(
        dll_module, "mpv_get_property_async");
    functions.observe_property = (mpv_observe_property)GetProcAddress(
        dll_module, "mpv_observe_property");
    functions.unobserve_property = (mpv_unobserve_property)GetProcAddress(
        dll_module, "mpv_unobserve_property");
    functions.event_name =
        (mpv_event_name)GetProcAddress(dll_module, "mpv_event_name");
    functions.event_to_node =
        (mpv_event_to_node)GetProcAddress(dll_module, "mpv_event_to_node");
    functions.request_event =
        (mpv_request_event)GetProcAddress(dll_module, "mpv_request_event");
    functions.request_log_messages = (mpv_request_log_messages)GetProcAddress(
        dll_module, "mpv_request_log_messages");
    functions.wait_event =
        (mpv_wait_event)GetProcAddress(dll_module, "mpv_wait_event");
    functions.wakeup = (mpv_wakeup)GetProcAddress(dll_module, "mpv_wakeup");
    functions.set_wakeup_callback = (mpv_set_wakeup_callback)GetProcAddress(
        dll_module, "mpv_set_wakeup_callback");
    functions.wait_async_requests = (mpv_wait_async_requests)GetProcAddress(
        dll_module, "mpv_wait_async_requests");
    functions.hook_add = (mpv_hook_add)GetProcAddress(dll_module, "mpv_hook_add");
    functions.hook_continue =
        (mpv_hook_continue)GetProcAddress(dll_module, "mpv_hook_continue");
    functions.stream_cb_add_ro = (mpv_stream_cb_add_ro)GetProcAddress(
        dll_module, "mpv_stream_cb_add_ro");

    functions.ready = true;
  }
};

static initquit_factory_t<libmpv_loader> g_mpv_loader;
}  // namespace libmpv
