#include "stdafx.h"
// PCH ^
#include "player.h"
#include "popup_window.h"

namespace mpv {
extern cfg_bool cfg_video_enabled, cfg_autopopup;
class mpv_play_callback : public play_callback_static {
  unsigned get_flags() {
    return play_callback::flag_on_playback_new_track |
           play_callback::flag_on_playback_pause |
           play_callback::flag_on_playback_seek |
           play_callback::flag_on_playback_starting |
           play_callback::flag_on_playback_stop |
           play_callback::flag_on_playback_time |
           play_callback::flag_on_volume_change;
  };
  void on_playback_starting(play_control::t_track_command p_command,
                            bool p_paused) {
    player::on_playback_starting(p_command, p_paused);
  };
  void on_playback_new_track(metadb_handle_ptr p_track) {
    pfc::string8 uri;
    bool should_play = player::should_play_this(p_track, uri);

    if (!cfg_video_enabled || (cfg_autopopup && !should_play)) {
      popup_window::close();
    }

    player::on_playback_new_track(p_track);

    if (cfg_video_enabled && cfg_autopopup && should_play) {
      popup_window::open(false);
    }
  };
  void on_playback_stop(play_control::t_stop_reason p_reason) {
    if (cfg_autopopup &&
        p_reason != play_control::t_stop_reason::stop_reason_starting_another) {
      popup_window::close();
    }
    player::on_playback_stop(p_reason);
  };
  void on_playback_seek(double p_time) { player::on_playback_seek(p_time); };
  void on_playback_pause(bool p_state) { player::on_playback_pause(p_state); };
  void on_playback_edited(metadb_handle_ptr p_track){};
  void on_playback_dynamic_info(const file_info& p_info){};
  void on_playback_dynamic_info_track(const file_info& p_info){};
  void on_playback_time(double p_time) { player::on_playback_time(p_time); };
  void on_volume_change(float p_new_val) {
    player::on_volume_change(p_new_val);
  };
};
play_callback_static_factory_t<mpv_play_callback> g_play_callback;
}  // namespace mpv
