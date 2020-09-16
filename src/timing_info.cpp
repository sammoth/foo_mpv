#include "stdafx.h"
// PCH ^
#include <atomic>

#include "timing_info.h"

namespace timing_info {
static std::atomic<timing_info> g_timing_info;

timing_info get() { return g_timing_info.load(); }

void force_refresh() {
  visualisation_stream::ptr vis_stream;
  visualisation_manager::get()->create_stream(vis_stream, 0);
  double vistime;
  vis_stream->get_absolute_time(vistime);
  double fb_time = playback_control::get()->playback_get_position();
  g_timing_info.store({0.0, vistime - fb_time});
}

class timing_info_callbacks : public play_callback_static {
  void on_playback_new_track(metadb_handle_ptr p_track) {
    visualisation_stream::ptr vis_stream;
    visualisation_manager::get()->create_stream(vis_stream, 0);
    double vistime;
    vis_stream->get_absolute_time(vistime);
    double fb_time = playback_control::get()->playback_get_position();
    g_timing_info.store({0.0, vistime - fb_time});
  }

  void on_playback_seek(double p_time) { g_timing_info.store({p_time, 0.0}); }
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused){};
  void on_playback_stop(play_control::t_stop_reason p_reason){};
  void on_playback_pause(bool p_state){};
  void on_playback_time(double p_time){};
  void on_playback_edited(metadb_handle_ptr p_track){};
  void on_playback_dynamic_info(const file_info& p_info){};
  void on_playback_dynamic_info_track(const file_info& p_info){};
  void on_volume_change(float p_new_val){};

  unsigned get_flags() {
    return play_callback::flag_on_playback_new_track |
           play_callback::flag_on_playback_seek;
  }
};

static service_factory_single_t<timing_info_callbacks> g_timing_info_callbacks;
}  // namespace timing_info
