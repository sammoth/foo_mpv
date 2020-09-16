#pragma once
#include "stdafx.h"
// PCH ^
#include <atomic>

namespace timing_info {
struct timing_info {
  double last_fb_seek;
  double last_seek_vistime;
};

timing_info get();
}  // namespace mpv
