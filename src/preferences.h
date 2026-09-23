#pragma once
#include "stdafx.h"
// PCH ^

namespace mpv {
enum class thumbnail_format : unsigned { Jpeg = 0, Png = 1 };

constexpr thumbnail_format thumbnail_format_from_config(unsigned value) {
  return value == static_cast<unsigned>(thumbnail_format::Png)
             ? thumbnail_format::Png
             : thumbnail_format::Jpeg;
}

void format_player_title(pfc::string8& s, metadb_handle_ptr metadb);
bool test_thumb_pattern(metadb_handle_ptr metadb);
bool test_video_pattern(metadb_handle_ptr metadb);
}  // namespace mpv
