#pragma once
#include "stdafx.h"
// PCH ^

#include <cstdint>

namespace mpv {
enum class thumbnail_format : unsigned { Jpeg = 0, Png = 1 };

constexpr thumbnail_format thumbnail_format_from_config(std::int64_t value) {
  return value == static_cast<std::int64_t>(thumbnail_format::Png)
             ? thumbnail_format::Png
             : thumbnail_format::Jpeg;
}

enum class artwork_type : unsigned { Front = 0, Back = 1, Disc = 2, Artist = 3 };

constexpr artwork_type artwork_type_from_config(std::int64_t value) {
  switch (value) {
    case static_cast<std::int64_t>(artwork_type::Back):
      return artwork_type::Back;
    case static_cast<std::int64_t>(artwork_type::Disc):
      return artwork_type::Disc;
    case static_cast<std::int64_t>(artwork_type::Artist):
      return artwork_type::Artist;
    default:
      return artwork_type::Front;
  }
}

void format_player_title(pfc::string8& s, metadb_handle_ptr metadb);
bool test_thumb_pattern(metadb_handle_ptr metadb);
bool test_video_pattern(metadb_handle_ptr metadb);
}  // namespace mpv
