#include "stdafx.h"
// PCH ^
#include <atomic>

#include "timing_info.h"

namespace timing_info {
static std::atomic<timing_info> g_timing_info;

timing_info get() { return g_timing_info.load(); }

void refresh(bool resetting) {
  double fb_time = playback_control::get()->playback_get_position();
  if (resetting) {
    g_timing_info.store({fb_time, 0.0});
  } else {
    visualisation_stream::ptr vis_stream;
    visualisation_manager::get()->create_stream(vis_stream, 0);
    double vistime;
    vis_stream->get_absolute_time(vistime);
    g_timing_info.store({fb_time, vistime});
  }
}
}  // namespace timing_info
