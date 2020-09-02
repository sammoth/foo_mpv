#include "stdafx.h"
#include "mpv.h"

#include <helpers/BumpableElem.h>

#include <sstream>

namespace {
	using namespace foo_mpv_h;

	static const GUID guid_mpv_panel =
	{ 0x777a523a, 0x1ed, 0x48b9, { 0xb9, 0x1, 0xda, 0xb1, 0xbe, 0x31, 0x7c, 0xa4 } };

	class timer {
		double last_seek;
		double mm_average_desync;
		int average_count;

	public:
		timer() : last_seek(0), mm_average_desync(0), average_count(0) {};

		double get_last_seek()
		{
			return last_seek;
		}

		double get_desync()
		{
			return mm_average_desync;
		}

		void seek(double seek)
		{
			last_seek = seek;
			mm_average_desync = 0;
			average_count = 0;
		}

		void record_desync(double desync)
		{
			if (average_count == 0)
			{
				mm_average_desync = desync;
				average_count++;
			}

			mm_average_desync = (mm_average_desync * (average_count - 1) + desync) / average_count;
			average_count = min(5, average_count + 1);
		}

		double correction_factor()
		{
			if (average_count < 1 || abs(mm_average_desync) < 0.08)
				return 1.0;

			return mm_average_desync > 0 ? 1.01 : 0.99;
		}

		bool should_sync()
		{
			return average_count > 3;
		}
	};

	struct CMpvWindow : public ui_element_instance, CWindowImpl<CMpvWindow>, play_callback_impl_base {
	public:
		DECLARE_WND_CLASS_EX(L"mpv_dui", CS_HREDRAW | CS_VREDRAW, 0)

		BEGIN_MSG_MAP(CMpvWindow)
			MSG_WM_DESTROY(kill_mpv)
		END_MSG_MAP()

		CMpvWindow(ui_element_config::ptr config,ui_element_instance_callback_ptr p_callback)
		: m_callback(p_callback), m_config(config), _timer()
		{
			if (!load_mpv())
				throw exception_messagebox("Could not load mpv");
		}

		void initialize_window(HWND parent)
		{
			Create(parent, 0, 0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);

			start_mpv();
		}

		void on_playback_starting(play_control::t_track_command p_command,bool p_paused) {
		}
		void on_playback_new_track(metadb_handle_ptr p_track) {
			_mpv_set_option_string(mpv, "start", "none");
			play_path(p_track->get_path());
		}
		void on_playback_stop(play_control::t_stop_reason p_reason) {
			stop();
		}
		void on_playback_seek(double p_time) {
			seek(p_time);
		}
		void on_playback_pause(bool p_state) {
			pause(p_state);
		}
		void on_playback_edited(metadb_handle_ptr p_track) {
			// path changed?
		}
		void on_playback_time(double p_time) {
			sync_maybe();
		}

		HWND get_wnd() {return m_hWnd;}

		void set_configuration(ui_element_config::ptr config) {m_config = config;}
		ui_element_config::ptr get_configuration() {return m_config;}
		static GUID g_get_guid() {
			return guid_mpv_panel;
		}
		static GUID g_get_subclass() {return ui_element_subclass_utility;}
		static void g_get_name(pfc::string_base & out) {out = "mpv";}
		static ui_element_config::ptr g_get_default_configuration() {return ui_element_config::g_create_empty(g_get_guid());}
		static const char * g_get_description() {return "mpv";}
		void notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size)
		{
			if (p_what == ui_element_notify_visibility_changed)
			{
				if (p_param1 == 1 && mpv == NULL)
				{
					start_mpv();
				}
				else if (p_param1 == 0 && mpv != NULL)
				{
					kill_mpv();
				}
			}
		};




	private:
		ui_element_config::ptr m_config;
		mpv_handle* mpv;

		timer _timer;

		void kill_mpv() {
			if (mpv != NULL)
			{
				mpv_handle* temp = mpv;
				mpv = NULL;
				_mpv_terminate_destroy(temp);
			}
		};

		void start_mpv()
		{
			pfc::string_formatter path;
			path.add_filename(core_api::get_profile_path());
			path.add_filename("mpv");
			path.replace_string("\\file://", "");

			mpv = _mpv_create();
			int64_t wid = (intptr_t)(m_hWnd);
			_mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

			_mpv_set_option_string(mpv, "config", "yes");
			_mpv_set_option_string(mpv, "config-dir", path.c_str());
			path.add_filename("mpv.log");
			_mpv_set_option_string(mpv, "log-file", path.c_str());

			// everything syncs to foobar
			_mpv_set_option_string(mpv, "video-sync", "audio");
			_mpv_set_option_string(mpv, "untimed", "no");

			// seek fast
			_mpv_set_option_string(mpv, "hr-seek-framedrop", "yes");

			// foobar plays the audio
			_mpv_set_option_string(mpv, "ao", "null");

			// seamless track change
			_mpv_set_option_string(mpv, "gapless-audio", "yes");

			// start timing audio immediately to keep in sync
			_mpv_set_option_string(mpv, "no-initial-audio-sync", "yes");

			// audio processing should use minimal cpu
			_mpv_set_option_string(mpv, "audio-resample-linear", "yes");
			_mpv_set_option_string(mpv, "audio-pitch-correction", "no");

			// keep the renderer initialised
			_mpv_set_option_string(mpv, "force-window", "yes");
			_mpv_set_option_string(mpv, "keep-open", "yes");
			_mpv_set_option_string(mpv, "idle", "yes");

			// load the next file a while in advance
			_mpv_set_option_string(mpv, "ao-null-buffer", "5");
			_mpv_set_option_string(mpv, "demuxer-readahead-secs", "0");

			// marked experimental, maybe unnecessary with gapless
			_mpv_set_option_string(mpv, "prefetch-playlist", "yes");

			_mpv_set_option_string(mpv, "video-latency-hacks", "yes");

			double start_time = 0;
			if (playback_control::get()->is_playing())
			{
				std::stringstream time_sstring;
				time_sstring.setf(std::ios::fixed);
				time_sstring.precision(3);
				start_time = playback_control::get()->playback_get_position();
				time_sstring << start_time;
				std::string time_string = time_sstring.str();
				_mpv_set_option_string(mpv, "start", time_string.c_str());
			}

			if (_mpv_initialize(mpv) != 0)
			{
				_mpv_terminate_destroy(mpv);
				mpv = NULL;
			}

			if (playback_control::get()->is_playing())
			{
				metadb_handle_ptr handle;
				playback_control::get()->get_now_playing(handle);
				play_path(handle->get_path());

				_timer.seek(start_time);
			}
		}

		void play_path(const char* metadb_path)
		{
			if (mpv == NULL)
				return;

			pfc::string8 filename;
			filename.add_filename(metadb_path);
			if (filename.has_prefix("\\file://"))
			{
				filename.remove_chars(0, 8);

				if (filename.is_empty())
					return;

				const char* cmd[] = { "loadfile", filename.c_str(), NULL };
				if (_mpv_command(mpv, cmd) < 0)
				{
					console::error("mpv: error loading file");
				}
			}

			_timer.seek(0);
		}

		void stop()
		{
			if (mpv == NULL)
				return;

			_mpv_command_string(mpv, "stop");
			_timer.seek(0);
		}

		void pause(bool state)
		{
			if (mpv == NULL)
				return;

			_mpv_set_property_string(mpv, "pause", state ? "yes" : "no");
		}

		void seek(double time)
		{
			if (mpv == NULL)
				return;

			std::stringstream time_sstring;
			time_sstring.setf(std::ios::fixed);
			time_sstring.precision(15);
			time_sstring << (time);
			std::string time_string = time_sstring.str();
			const char* cmd[] = { "seek", time_string.c_str(), "absolute+exact", NULL };
			if (_mpv_command(mpv, cmd) < 0)
			{
				console::error("mpv: error seeking");
			}

			_timer.seek(time);
		}

		void sync_maybe()
		{
			if (mpv == NULL)
				return;

			double mpv_time = -1.0;
			if (_mpv_get_property(mpv, "audio-pts", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
				return;
			double fb_time = playback_control::get()->playback_get_position();

			_timer.record_desync(fb_time - mpv_time);

			if (_timer.should_sync())
			{
				if (abs(_timer.get_desync()) > 0.4)
				{
					// hard sync
					seek(playback_control::get()->playback_get_position());
					console::info("mpv: A/V resynced");
				}
				else
				{
					// soft sync
					double scale = _timer.correction_factor();
					_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &scale);
				}
			}
		}


	protected:
		const ui_element_instance_callback_ptr m_callback;
	};
	class ui_element_mpvimpl : public ui_element_impl_withpopup<CMpvWindow> {};
	static service_factory_single_t<ui_element_mpvimpl> g_ui_element_mpvimpl_factory;
} // namespace

