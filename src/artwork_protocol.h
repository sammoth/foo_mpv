#pragma once
#include "stdafx.h"
// PCH ^

#include "libmpv.h"

namespace mpv {
int artwork_protocol_open(void* user_data, char* uri,
                                 libmpv::mpv_stream_cb_info* info);
void request_artwork(metadb_handle_list_cref items);
void request_artwork();
void reload_artwork();
metadb_handle_ptr single_artwork_item();
bool artwork_loaded();
}
