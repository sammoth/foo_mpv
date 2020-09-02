#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sstream>

namespace foo_mpv_h {
	typedef unsigned long(__cdecl* mpv_client_api_version)();
	typedef struct mpv_handle mpv_handle;
	typedef enum mpv_error {
		MPV_ERROR_SUCCESS = 0,
		MPV_ERROR_EVENT_QUEUE_FULL = -1,
		MPV_ERROR_NOMEM = -2,
		MPV_ERROR_UNINITIALIZED = -3,
		MPV_ERROR_INVALID_PARAMETER = -4,
		MPV_ERROR_OPTION_NOT_FOUND = -5,
		MPV_ERROR_OPTION_FORMAT = -6,
		MPV_ERROR_OPTION_ERROR = -7,
		MPV_ERROR_PROPERTY_NOT_FOUND = -8,
		MPV_ERROR_PROPERTY_FORMAT = -9,
		MPV_ERROR_PROPERTY_UNAVAILABLE = -10,
		MPV_ERROR_PROPERTY_ERROR = -11,
		MPV_ERROR_COMMAND = -12,
		MPV_ERROR_LOADING_FAILED = -13,
		MPV_ERROR_AO_INIT_FAILED = -14,
		MPV_ERROR_VO_INIT_FAILED = -15,
		MPV_ERROR_NOTHING_TO_PLAY = -16,
		MPV_ERROR_UNKNOWN_FORMAT = -17,
		MPV_ERROR_UNSUPPORTED = -18,
		MPV_ERROR_NOT_IMPLEMENTED = -19,
		MPV_ERROR_GENERIC = -20
	} mpv_error;
	typedef const char* (__cdecl* mpv_error_string)(int error);
	mpv_error_string _mpv_error_string;
	typedef void(__cdecl* mpv_free)(void* data);
	mpv_free _mpv_free;
	typedef const char* (__cdecl* mpv_client_name)(mpv_handle* ctx);
	mpv_client_name _mpv_client_name;
	typedef int64_t(__cdecl* mpv_client_id)(mpv_handle* ctx);
	mpv_client_id _mpv_client_id;
	typedef mpv_handle* (__cdecl* mpv_create)(void);
	mpv_create _mpv_create;
	typedef int(__cdecl* mpv_initialize)(mpv_handle* ctx);
	mpv_initialize _mpv_initialize;
	typedef void(__cdecl* mpv_destroy)(mpv_handle* ctx);
	mpv_destroy _mpv_destroy;
	typedef void(__cdecl* mpv_terminate_destroy)(mpv_handle* ctx);
	mpv_terminate_destroy _mpv_terminate_destroy;
	typedef mpv_handle* (__cdecl* mpv_create_client)(mpv_handle* ctx, const char* name);
	mpv_create_client _mpv_create_client;
	typedef mpv_handle* (__cdecl* mpv_create_weak_client)(mpv_handle* ctx, const char* name);
	mpv_create_weak_client _mpv_create_weak_client;
	typedef int(__cdecl* mpv_load_config_file)(mpv_handle* ctx, const char* filename);
	mpv_load_config_file _mpv_load_config_file;
	typedef int64_t(__cdecl* mpv_get_time_us)(mpv_handle* ctx);
	mpv_get_time_us _mpv_get_time_us;
	typedef enum mpv_format {
		MPV_FORMAT_NONE = 0,
		MPV_FORMAT_STRING = 1,
		MPV_FORMAT_OSD_STRING = 2,
		MPV_FORMAT_FLAG = 3,
		MPV_FORMAT_INT64 = 4,
		MPV_FORMAT_DOUBLE = 5,
		MPV_FORMAT_NODE = 6,
		MPV_FORMAT_NODE_ARRAY = 7,
		MPV_FORMAT_NODE_MAP = 8,
		MPV_FORMAT_BYTE_ARRAY = 9
	} mpv_format;
	typedef struct mpv_node {
		union {
			char* string;
			int flag;
			int64_t int64;
			double double_;
			struct mpv_node_list* list;
			struct mpv_byte_array* ba;
		} u;
		mpv_format format;
	} mpv_node;
	typedef struct mpv_node_list {
		int num;
		mpv_node* values;
		char** keys;
	} mpv_node_list;
	typedef struct mpv_byte_array {
		void* data;
		size_t size;
	} mpv_byte_array;
	typedef void(__cdecl* mpv_free_node_contents)(mpv_node* node);
	mpv_free_node_contents _mpv_free_node_contents;
	typedef int(__cdecl* mpv_set_option)(mpv_handle* ctx, const char* name, mpv_format format,
		void* data);
	mpv_set_option _mpv_set_option;
	typedef int(__cdecl* mpv_set_option_string)(mpv_handle* ctx, const char* name, const char* data);
	mpv_set_option_string _mpv_set_option_string;
	typedef int(__cdecl* mpv_command)(mpv_handle* ctx, const char** args);
	mpv_command _mpv_command;
	typedef int(__cdecl* mpv_command_node)(mpv_handle* ctx, mpv_node* args, mpv_node* result);
	mpv_command_node _mpv_command_node;
	typedef int(__cdecl* mpv_command_ret)(mpv_handle* ctx, const char** args, mpv_node* result);
	mpv_command_ret _mpv_command_ret;
	typedef int(__cdecl* mpv_command_string)(mpv_handle* ctx, const char* args);
	mpv_command_string _mpv_command_string;
	typedef int(__cdecl* mpv_command_async)(mpv_handle* ctx, uint64_t reply_userdata,
		const char** args);
	mpv_command_async _mpv_command_async;
	typedef int(__cdecl* mpv_command_node_async)(mpv_handle* ctx, uint64_t reply_userdata,
		mpv_node* args);
	mpv_command_node_async _mpv_command_node_async;
	typedef void(__cdecl* mpv_abort_async_command)(mpv_handle* ctx, uint64_t reply_userdata);
	mpv_abort_async_command _mpv_abort_async_command;
	typedef int(__cdecl* mpv_set_property)(mpv_handle* ctx, const char* name, mpv_format format,
		void* data);
	mpv_set_property _mpv_set_property;
	typedef int(__cdecl* mpv_set_property_string)(mpv_handle* ctx, const char* name, const char* data);
	mpv_set_property_string _mpv_set_property_string;
	typedef int(__cdecl* mpv_set_property_async)(mpv_handle* ctx, uint64_t reply_userdata,
		const char* name, mpv_format format, void* data);
	mpv_set_property_async _mpv_set_property_async;
	typedef int(__cdecl* mpv_get_property)(mpv_handle* ctx, const char* name, mpv_format format,
		void* data);
	mpv_get_property _mpv_get_property;
	typedef char* (__cdecl* mpv_get_property_string)(mpv_handle* ctx, const char* name);
	mpv_get_property_string _mpv_get_property_string;
	typedef char* (__cdecl* mpv_get_property_osd_string)(mpv_handle* ctx, const char* name);
	mpv_get_property_osd_string _mpv_get_property_osd_string;
	typedef int(__cdecl* mpv_get_property_async)(mpv_handle* ctx, uint64_t reply_userdata,
		const char* name, mpv_format format);
	mpv_get_property_async _mpv_get_property_async;
	typedef int(__cdecl* mpv_observe_property)(mpv_handle* mpv, uint64_t reply_userdata,
		const char* name, mpv_format format);
	mpv_observe_property _mpv_observe_property;
	typedef int(__cdecl* mpv_unobserve_property)(mpv_handle* mpv, uint64_t registered_reply_userdata);
	mpv_unobserve_property _mpv_unobserve_property;
	typedef enum mpv_event_id {
		MPV_EVENT_NONE = 0,
		MPV_EVENT_SHUTDOWN = 1,
		MPV_EVENT_LOG_MESSAGE = 2,
		MPV_EVENT_GET_PROPERTY_REPLY = 3,
		MPV_EVENT_SET_PROPERTY_REPLY = 4,
		MPV_EVENT_COMMAND_REPLY = 5,
		MPV_EVENT_START_FILE = 6,
		MPV_EVENT_END_FILE = 7,
		MPV_EVENT_FILE_LOADED = 8,
		MPV_EVENT_CLIENT_MESSAGE = 16,
		MPV_EVENT_VIDEO_RECONFIG = 17,
		MPV_EVENT_AUDIO_RECONFIG = 18,
		MPV_EVENT_SEEK = 20,
		MPV_EVENT_PLAYBACK_RESTART = 21,
		MPV_EVENT_PROPERTY_CHANGE = 22,
		MPV_EVENT_QUEUE_OVERFLOW = 24,
		MPV_EVENT_HOOK = 25,
	} mpv_event_id;
	typedef const char* (__cdecl* mpv_event_name)(mpv_event_id event);
	mpv_event_name _mpv_event_name;
	typedef struct mpv_event_property {
		const char* name;
		mpv_format format;
		void* data;
	} mpv_event_property;
	typedef enum mpv_log_level {
		MPV_LOG_LEVEL_NONE = 0,    /// "no"    - disable absolutely all messages
		MPV_LOG_LEVEL_FATAL = 10,   /// "fatal" - critical/aborting errors
		MPV_LOG_LEVEL_ERROR = 20,   /// "error" - simple errors
		MPV_LOG_LEVEL_WARN = 30,   /// "warn"  - possible problems
		MPV_LOG_LEVEL_INFO = 40,   /// "info"  - informational message
		MPV_LOG_LEVEL_V = 50,   /// "v"     - noisy informational message
		MPV_LOG_LEVEL_DEBUG = 60,   /// "debug" - very noisy technical information
		MPV_LOG_LEVEL_TRACE = 70,   /// "trace" - extremely noisy
	} mpv_log_level;
	typedef struct mpv_event_log_message {
		const char* prefix;
		const char* level;
		const char* text;
		mpv_log_level log_level;
	} mpv_event_log_message;
	typedef enum mpv_end_file_reason {
		MPV_END_FILE_REASON_EOF = 0,
		MPV_END_FILE_REASON_STOP = 2,
		MPV_END_FILE_REASON_QUIT = 3,
		MPV_END_FILE_REASON_ERROR = 4,
		MPV_END_FILE_REASON_REDIRECT = 5,
	} mpv_end_file_reason;
	typedef struct mpv_event_start_file {
		int64_t playlist_entry_id;
	} mpv_event_start_file;
	typedef struct mpv_event_end_file {
		int reason;
		int error;
		int64_t playlist_entry_id;
		int64_t playlist_insert_id;
		int playlist_insert_num_entries;
	} mpv_event_end_file;
	typedef struct mpv_event_client_message {
		int num_args;
		const char** args;
	} mpv_event_client_message;
	typedef struct mpv_event_hook {
		const char* name;
		uint64_t id;
	} mpv_event_hook;
	// Since API version 1.102.
	typedef struct mpv_event_command {
		mpv_node result;
	} mpv_event_command;
	typedef struct mpv_event {
		mpv_event_id event_id;
		int error;
		uint64_t reply_userdata;
		void* data;
	} mpv_event;
	typedef int(__cdecl* mpv_event_to_node)(mpv_node* dst, mpv_event* src);
	mpv_event_to_node _mpv_event_to_node;
	typedef int(__cdecl* mpv_request_event)(mpv_handle* ctx, mpv_event_id event, int enable);
	mpv_request_event _mpv_request_event;
	typedef int(__cdecl* mpv_request_log_messages)(mpv_handle* ctx, const char* min_level);
	mpv_request_log_messages _mpv_request_log_messages;
	typedef mpv_event* (__cdecl* mpv_wait_event)(mpv_handle* ctx, double timeout);
	mpv_wait_event _mpv_wait_event;
	typedef void(__cdecl* mpv_wakeup)(mpv_handle* ctx);
	mpv_wakeup _mpv_wakeup;
	typedef void(__cdecl* mpv_set_wakeup_callback)(mpv_handle* ctx, void (*cb)(void* d), void* d);
	mpv_set_wakeup_callback _mpv_set_wakeup_callback;
	typedef void(__cdecl* mpv_wait_async_requests)(mpv_handle* ctx);
	mpv_wait_async_requests _mpv_wait_async_requests;
	typedef int(__cdecl* mpv_hook_add)(mpv_handle* ctx, uint64_t reply_userdata,
		const char* name, int priority);
	mpv_hook_add _mpv_hook_add;
	typedef int(__cdecl* mpv_hook_continue)(mpv_handle* ctx, uint64_t id);
	mpv_hook_continue _mpv_hook_continue;

	bool load_mpv()
	{
		HMODULE dll;
		pfc::string_formatter path = core_api::get_my_full_path();
		path.truncate(path.scan_filename());
		std::wstringstream wpath_mpv;
		wpath_mpv << path << "mpv\\mpv-1.dll";
		dll = LoadLibraryExW(wpath_mpv.str().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (dll == NULL)
		{
			std::stringstream error;
			error << "Could not load mpv-1.dll: error code " << GetLastError();
			console::error(error.str().c_str());
			return false;
		}

		_mpv_error_string = (mpv_error_string)GetProcAddress(dll, "mpv_error_string");
		_mpv_free = (mpv_free)GetProcAddress(dll, "mpv_free");
		_mpv_client_name = (mpv_client_name)GetProcAddress(dll, "mpv_client_name");
		_mpv_client_id = (mpv_client_id)GetProcAddress(dll, "mpv_client_id");
		_mpv_create = (mpv_create)GetProcAddress(dll, "mpv_create");
		_mpv_initialize = (mpv_initialize)GetProcAddress(dll, "mpv_initialize");
		_mpv_destroy = (mpv_destroy)GetProcAddress(dll, "mpv_destroy");
		_mpv_terminate_destroy = (mpv_terminate_destroy)GetProcAddress(dll, "mpv_terminate_destroy");
		_mpv_create_client = (mpv_create_client)GetProcAddress(dll, "mpv_create_client");
		_mpv_create_weak_client = (mpv_create_weak_client)GetProcAddress(dll, "mpv_create_weak_client");
		_mpv_load_config_file = (mpv_load_config_file)GetProcAddress(dll, "mpv_load_config_file");
		_mpv_get_time_us = (mpv_get_time_us)GetProcAddress(dll, "mpv_get_time_us");
		_mpv_free_node_contents = (mpv_free_node_contents)GetProcAddress(dll, "mpv_free_node_contents");
		_mpv_set_option = (mpv_set_option)GetProcAddress(dll, "mpv_set_option");
		_mpv_set_option_string = (mpv_set_option_string)GetProcAddress(dll, "mpv_set_option_string");
		_mpv_command = (mpv_command)GetProcAddress(dll, "mpv_command");
		_mpv_command_node = (mpv_command_node)GetProcAddress(dll, "mpv_command_node");
		_mpv_command_ret = (mpv_command_ret)GetProcAddress(dll, "mpv_command_ret");
		_mpv_command_string = (mpv_command_string)GetProcAddress(dll, "mpv_command_string");
		_mpv_command_async = (mpv_command_async)GetProcAddress(dll, "mpv_command_async");
		_mpv_command_node_async = (mpv_command_node_async)GetProcAddress(dll, "mpv_command_node_async");
		_mpv_abort_async_command = (mpv_abort_async_command)GetProcAddress(dll, "mpv_abort_async_command");
		_mpv_set_property = (mpv_set_property)GetProcAddress(dll, "mpv_set_property");
		_mpv_set_property_string = (mpv_set_property_string)GetProcAddress(dll, "mpv_set_property_string");
		_mpv_set_property_async = (mpv_set_property_async)GetProcAddress(dll, "mpv_set_property_async");
		_mpv_get_property = (mpv_get_property)GetProcAddress(dll, "mpv_get_property");
		_mpv_get_property_string = (mpv_get_property_string)GetProcAddress(dll, "mpv_get_property_string");
		_mpv_get_property_osd_string = (mpv_get_property_osd_string)GetProcAddress(dll, "mpv_get_property_osd_string");
		_mpv_get_property_async = (mpv_get_property_async)GetProcAddress(dll, "mpv_get_property_async");
		_mpv_observe_property = (mpv_observe_property)GetProcAddress(dll, "mpv_observe_property");
		_mpv_unobserve_property = (mpv_unobserve_property)GetProcAddress(dll, "mpv_unobserve_property");
		_mpv_event_name = (mpv_event_name)GetProcAddress(dll, "mpv_event_name");
		_mpv_event_to_node = (mpv_event_to_node)GetProcAddress(dll, "mpv_event_to_node");
		_mpv_request_event = (mpv_request_event)GetProcAddress(dll, "mpv_request_event");
		_mpv_request_log_messages = (mpv_request_log_messages)GetProcAddress(dll, "mpv_request_log_messages");
		_mpv_wait_event = (mpv_wait_event)GetProcAddress(dll, "mpv_wait_event");
		_mpv_wakeup = (mpv_wakeup)GetProcAddress(dll, "mpv_wakeup");
		_mpv_set_wakeup_callback = (mpv_set_wakeup_callback)GetProcAddress(dll, "mpv_set_wakeup_callback");
		_mpv_wait_async_requests = (mpv_wait_async_requests)GetProcAddress(dll, "mpv_wait_async_requests");
		_mpv_hook_add = (mpv_hook_add)GetProcAddress(dll, "mpv_hook_add");
		_mpv_hook_continue = (mpv_hook_continue)GetProcAddress(dll, "mpv_hook_continue");

		return true;
	}
}

