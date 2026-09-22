#pragma once
#include "stdafx.h"
// PCH ^
#include <optional>

namespace timing_info {
struct timing_info {
  double last_fb_seek;
  double last_seek_vistime;
};

std::optional<timing_info> get();
bool refresh(bool resetting);
}  // namespace timing_info
