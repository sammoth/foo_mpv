#pragma once
#include "stdafx.h"
// PCH ^

#include "libmpv.h"

namespace mpv {
int artwork_protocol_open(void* user_data, char* uri,
                                 libmpv::mpv_stream_cb_info* info);
void request_artwork(metadb_handle_ptr item);
void reload_artwork();
bool artwork_loaded();
}
