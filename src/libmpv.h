#pragma once
#include "stdafx.h"
// PCH ^

/* Copyright (C) 2017 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

namespace libmpv {
pfc::string8 get_version();
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
typedef const char*(__cdecl* mpv_error_string)(int error);
typedef void(__cdecl* mpv_free)(void* data);
typedef const char*(__cdecl* mpv_client_name)(mpv_handle* ctx);
typedef int64_t(__cdecl* mpv_client_id)(mpv_handle* ctx);
typedef mpv_handle*(__cdecl* mpv_create)(void);
typedef int(__cdecl* mpv_initialize)(mpv_handle* ctx);
typedef void(__cdecl* mpv_destroy)(mpv_handle* ctx);
typedef void(__cdecl* mpv_terminate_destroy)(mpv_handle* ctx);
typedef mpv_handle*(__cdecl* mpv_create_client)(mpv_handle* ctx,
                                                const char* name);
typedef mpv_handle*(__cdecl* mpv_create_weak_client)(mpv_handle* ctx,
                                                     const char* name);
typedef int(__cdecl* mpv_load_config_file)(mpv_handle* ctx,
                                           const char* filename);
typedef int64_t(__cdecl* mpv_get_time_us)(mpv_handle* ctx);
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
typedef int(__cdecl* mpv_set_option)(mpv_handle* ctx, const char* name,
                                     mpv_format format, void* data);
typedef int(__cdecl* mpv_set_option_string)(mpv_handle* ctx, const char* name,
                                            const char* data);
typedef int(__cdecl* mpv_command)(mpv_handle* ctx, const char** args);
typedef int(__cdecl* mpv_command_node)(mpv_handle* ctx, mpv_node* args,
                                       mpv_node* result);
typedef int(__cdecl* mpv_command_ret)(mpv_handle* ctx, const char** args,
                                      mpv_node* result);
typedef int(__cdecl* mpv_command_string)(mpv_handle* ctx, const char* args);
typedef int(__cdecl* mpv_command_async)(mpv_handle* ctx,
                                        uint64_t reply_userdata,
                                        const char** args);
typedef int(__cdecl* mpv_command_node_async)(mpv_handle* ctx,
                                             uint64_t reply_userdata,
                                             mpv_node* args);
typedef void(__cdecl* mpv_abort_async_command)(mpv_handle* ctx,
                                               uint64_t reply_userdata);
typedef int(__cdecl* mpv_set_property)(mpv_handle* ctx, const char* name,
                                       mpv_format format, void* data);
typedef int(__cdecl* mpv_set_property_string)(mpv_handle* ctx, const char* name,
                                              const char* data);
typedef int(__cdecl* mpv_set_property_async)(mpv_handle* ctx,
                                             uint64_t reply_userdata,
                                             const char* name,
                                             mpv_format format, void* data);
typedef int(__cdecl* mpv_get_property)(mpv_handle* ctx, const char* name,
                                       mpv_format format, void* data);
typedef char*(__cdecl* mpv_get_property_string)(mpv_handle* ctx,
                                                const char* name);
typedef char*(__cdecl* mpv_get_property_osd_string)(mpv_handle* ctx,
                                                    const char* name);
typedef int(__cdecl* mpv_get_property_async)(mpv_handle* ctx,
                                             uint64_t reply_userdata,
                                             const char* name,
                                             mpv_format format);
typedef int(__cdecl* mpv_observe_property)(mpv_handle* mpv,
                                           uint64_t reply_userdata,
                                           const char* name, mpv_format format);
typedef int(__cdecl* mpv_unobserve_property)(
    mpv_handle* mpv, uint64_t registered_reply_userdata);
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
  MPV_EVENT_TICK = 14,
  MPV_EVENT_CLIENT_MESSAGE = 16,
  MPV_EVENT_VIDEO_RECONFIG = 17,
  MPV_EVENT_AUDIO_RECONFIG = 18,
  MPV_EVENT_SEEK = 20,
  MPV_EVENT_PLAYBACK_RESTART = 21,
  MPV_EVENT_PROPERTY_CHANGE = 22,
  MPV_EVENT_QUEUE_OVERFLOW = 24,
  MPV_EVENT_HOOK = 25,
} mpv_event_id;
typedef const char*(__cdecl* mpv_event_name)(mpv_event_id event);
typedef struct mpv_event_property {
  const char* name;
  mpv_format format;
  void* data;
} mpv_event_property;
typedef enum mpv_log_level {
  MPV_LOG_LEVEL_NONE = 0,    /// "no"    - disable absolutely all messages
  MPV_LOG_LEVEL_FATAL = 10,  /// "fatal" - critical/aborting errors
  MPV_LOG_LEVEL_ERROR = 20,  /// "error" - simple errors
  MPV_LOG_LEVEL_WARN = 30,   /// "warn"  - possible problems
  MPV_LOG_LEVEL_INFO = 40,   /// "info"  - informational message
  MPV_LOG_LEVEL_V = 50,      /// "v"     - noisy informational message
  MPV_LOG_LEVEL_DEBUG = 60,  /// "debug" - very noisy technical information
  MPV_LOG_LEVEL_TRACE = 70,  /// "trace" - extremely noisy
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
typedef int(__cdecl* mpv_request_event)(mpv_handle* ctx, mpv_event_id event,
                                        int enable);
typedef int(__cdecl* mpv_request_log_messages)(mpv_handle* ctx,
                                               const char* min_level);
typedef mpv_event*(__cdecl* mpv_wait_event)(mpv_handle* ctx, double timeout);
typedef void(__cdecl* mpv_wakeup)(mpv_handle* ctx);
typedef void(__cdecl* mpv_set_wakeup_callback)(mpv_handle* ctx,
                                               void (*cb)(void* d), void* d);
typedef void(__cdecl* mpv_wait_async_requests)(mpv_handle* ctx);
typedef int(__cdecl* mpv_hook_add)(mpv_handle* ctx, uint64_t reply_userdata,
                                   const char* name, int priority);
typedef int(__cdecl* mpv_hook_continue)(mpv_handle* ctx, uint64_t id);

typedef int64_t(__cdecl* mpv_stream_cb_read_fn)(void* cookie, char* buf,
                                                uint64_t nbytes);
typedef int64_t(__cdecl* mpv_stream_cb_seek_fn)(void* cookie, int64_t offset);
typedef int64_t(__cdecl* mpv_stream_cb_size_fn)(void* cookie);
typedef void(__cdecl* mpv_stream_cb_close_fn)(void* cookie);
typedef void(__cdecl* mpv_stream_cb_cancel_fn)(void* cookie);
typedef struct mpv_stream_cb_info {
  void* cookie;
  mpv_stream_cb_read_fn read_fn;
  mpv_stream_cb_seek_fn seek_fn;
  mpv_stream_cb_size_fn size_fn;
  mpv_stream_cb_close_fn close_fn;
  mpv_stream_cb_cancel_fn cancel_fn;
} mpv_stream_cb_info;
typedef int(__cdecl* mpv_stream_cb_open_ro_fn)(void* user_data, char* uri,
                                               mpv_stream_cb_info* info);
typedef int(__cdecl* mpv_stream_cb_add_ro)(mpv_handle* ctx,
                                           const char* protocol,
                                           void* user_data,
                                           mpv_stream_cb_open_ro_fn open_fn);

struct function_table {
  bool ready = false;
  mpv_error_string error_string;
  mpv_free free;
  mpv_client_name client_name;
  mpv_client_id client_id;
  mpv_create create;
  mpv_initialize initialize;
  mpv_destroy destroy;
  mpv_terminate_destroy terminate_destroy;
  mpv_create_client create_client;
  mpv_create_weak_client create_weak_client;
  mpv_load_config_file load_config_file;
  mpv_get_time_us get_time_us;
  mpv_free_node_contents free_node_contents;
  mpv_set_option set_option;
  mpv_set_option_string set_option_string;
  mpv_command command;
  mpv_command_node command_node;
  mpv_command_ret command_ret;
  mpv_command_string command_string;
  mpv_command_async command_async;
  mpv_command_node_async command_node_async;
  mpv_abort_async_command abort_async_command;
  mpv_set_property set_property;
  mpv_set_property_string set_property_string;
  mpv_set_property_async set_property_async;
  mpv_get_property get_property;
  mpv_get_property_string get_property_string;
  mpv_get_property_osd_string get_property_osd_string;
  mpv_get_property_async get_property_async;
  mpv_observe_property observe_property;
  mpv_unobserve_property unobserve_property;
  mpv_event_name event_name;
  mpv_event_to_node event_to_node;
  mpv_request_event request_event;
  mpv_request_log_messages request_log_messages;
  mpv_wait_event wait_event;
  mpv_wakeup wakeup;
  mpv_set_wakeup_callback set_wakeup_callback;
  mpv_wait_async_requests wait_async_requests;
  mpv_hook_add hook_add;
  mpv_hook_continue hook_continue;
  mpv_stream_cb_add_ro stream_cb_add_ro;
};

function_table* get();
}  // namespace libmpv
