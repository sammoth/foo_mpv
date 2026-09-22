#include "stdafx.h"
// PCH ^
#include <atomic>
#include <cmath>
#include <limits>

#include "timing_info.h"

namespace timing_info {
static std::atomic<timing_info> g_timing_info{
    timing_info{0.0, std::numeric_limits<double>::quiet_NaN()}};

std::optional<timing_info> get() {
  timing_info value = g_timing_info.load();
  if (std::isnan(value.last_seek_vistime)) return std::nullopt;
  return value;
}

bool refresh(bool resetting) {
  double fb_time = playback_control::get()->playback_get_position();
  if (resetting) {
    g_timing_info.store({fb_time, 0.0});
    return true;
  }

  visualisation_stream::ptr vis_stream;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  double vistime = 0.0;
  if (!vis_stream.is_valid() || !vis_stream->get_absolute_time(vistime)) {
    g_timing_info.store({fb_time,
                         std::numeric_limits<double>::quiet_NaN()});
    return false;
  }
  g_timing_info.store({fb_time, vistime});
  return true;
}
}  // namespace timing_info
