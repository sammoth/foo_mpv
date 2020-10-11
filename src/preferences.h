#pragma once
#include "stdafx.h"
// PCH ^

namespace mpv {
void format_player_title(pfc::string8& s, metadb_handle_ptr metadb);
bool test_thumb_pattern(metadb_handle_ptr metadb);
bool test_video_pattern(metadb_handle_ptr metadb);
}  // namespace mpv
