#include "stdafx.h"
#include "mpv.h"

#include <helpers/BumpableElem.h>

#include <sstream>

namespace {
	using namespace foo_mpv_h;

	static const GUID guid_cfg_mpv_branch =
	{ 0xa8d3b2ca, 0xa9a, 0x4efc, { 0xa4, 0x33, 0x32, 0x4d, 0x76, 0xcc, 0x8a, 0x33 } };
	static const GUID guid_cfg_mpv_max_drift =
	{ 0xa799d117, 0x7e68, 0x4d1e, { 0x9d, 0xc2, 0xe8, 0x16, 0x1e, 0xf1, 0xf5, 0xfe } };
	static const GUID guid_cfg_mpv_hard_sync =
	{ 0x240d9ab0, 0xb58d, 0x4565, { 0x9e, 0xc0, 0x6b, 0x27, 0x99, 0xcd, 0x2d, 0xed } };
	static const GUID guid_cfg_mpv_logging =
	{ 0x8b74d741, 0x232a, 0x46d5, { 0xa7, 0xee, 0x4, 0x89, 0xb1, 0x47, 0x43, 0xf0 } };
	static const GUID guid_cfg_mpv_native_logging =
	{ 0x3411741c, 0x239, 0x441d, { 0x8a, 0x8e, 0x99, 0x83, 0x2a, 0xda, 0xe7, 0xd0 } };
	static const GUID guid_cfg_mpv_stop_hidden =
	{ 0x9de7e631, 0x64f8, 0x4047, { 0x88, 0x39, 0x8f, 0x4a, 0x50, 0xa0, 0xb7, 0x2f } };



	static advconfig_branch_factory g_mpv_branch("Mpv", guid_cfg_mpv_branch, advconfig_branch::guid_branch_playback, 0);
	static advconfig_integer_factory cfg_mpv_max_drift("Permitted timing drift (ms)", guid_cfg_mpv_max_drift, guid_cfg_mpv_branch, 0, 20, 0, 1000, 0);
	static advconfig_integer_factory cfg_mpv_hard_sync("Hard sync threshold (ms)", guid_cfg_mpv_hard_sync, guid_cfg_mpv_branch, 0, 2000, 0, 10000, 0);
	static advconfig_checkbox_factory cfg_mpv_logging("Enable verbose console logging", guid_cfg_mpv_logging, guid_cfg_mpv_branch, 0, false);
	static advconfig_checkbox_factory cfg_mpv_native_logging("Enable mpv log file", guid_cfg_mpv_native_logging, guid_cfg_mpv_branch, 0, false);
	static advconfig_checkbox_factory cfg_mpv_stop_hidden("Stop when hidden", guid_cfg_mpv_stop_hidden, guid_cfg_mpv_branch, 0, true);

	static const GUID guid_mpv_panel =
	{ 0x777a523a, 0x1ed, 0x48b9, { 0xb9, 0x1, 0xda, 0xb1, 0xbe, 0x31, 0x7c, 0xa4 } };

	struct CMpvWindow : public ui_element_instance, CWindowImpl<CMpvWindow>, play_callback_impl_base {
	public:
		DECLARE_WND_CLASS_EX(L"mpv_dui", CS_HREDRAW | CS_VREDRAW, 0)

		BEGIN_MSG_MAP(CMpvWindow)
			MSG_WM_ERASEBKGND(erase_bg)
			MSG_WM_DESTROY(kill_mpv)
		END_MSG_MAP()

		CMpvWindow(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback)
			: m_callback(p_callback), m_config(config)
		{
			if (!load_mpv())
			{
				console::error("Could not load mpv-1.dll");
				throw exception_messagebox("Could not load mpv-1.dll");
			}
		}

		BOOL erase_bg(CDCHandle dc) {
			CRect rc; WIN32_OP_D( GetClientRect(&rc) );
			CBrush brush;
			WIN32_OP_D( brush.CreateSolidBrush(0x00000000) != NULL );
			WIN32_OP_D( dc.FillRect(&rc, brush) );
			return TRUE;
		}

		void initialize_window(HWND parent)
		{
			Create(parent, 0, 0, WS_CHILD, 0);
		}

		void on_playback_starting(play_control::t_track_command p_command, bool p_paused) {
		}
		void on_playback_new_track(metadb_handle_ptr p_track) {
			if (enabled)
				play_path(p_track->get_path());
		}
		void on_playback_stop(play_control::t_stop_reason p_reason) {
			stop();
		}
		void on_playback_seek(double p_time) {
			if (enabled)
				seek(p_time);
		}
		void on_playback_pause(bool p_state) {
			if (enabled)
				pause(p_state);
		}
		void on_playback_edited(metadb_handle_ptr p_track) {
		}
		void on_playback_time(double p_time) {
			if (enabled)
				sync();
		}

		HWND get_wnd() { return m_hWnd; }

		void set_configuration(ui_element_config::ptr config) { m_config = config; }
		ui_element_config::ptr get_configuration() { return m_config; }
		static GUID g_get_guid() {
			return guid_mpv_panel;
		}
		static GUID g_get_subclass() { return ui_element_subclass_utility; }
		static void g_get_name(pfc::string_base& out) { out = "Mpv"; }
		static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
		static const char* g_get_description() { return "Mpv"; }
		void notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size)
		{
			if (p_what == ui_element_notify_visibility_changed)
			{
				if (cfg_mpv_stop_hidden)
				{
					enabled = p_param1 == 1;

					if (enabled)
					{
						start_mpv();
					}
					else
					{
						stop();
					}
				}
				else if (p_param1 == 1)
				{
					if (!enabled)
						start_mpv();

					enabled = true;
				}
			}
		};




	private:
		ui_element_config::ptr m_config;
		mpv_handle* mpv = NULL;

		bool enabled = false;

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

			if (mpv == NULL)
			{
				pfc::string_formatter path;
				path.add_filename(core_api::get_profile_path());
				path.add_filename("mpv");
				path.replace_string("\\file://", "");
				mpv = _mpv_create();

				int64_t wid = (intptr_t)(m_hWnd);
				_mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

				_mpv_set_option_string(mpv, "load-scripts", "no");
				_mpv_set_option_string(mpv, "ytdl", "no");
				_mpv_set_option_string(mpv, "load-stats-overlay", "no");
				_mpv_set_option_string(mpv, "load-osd-console", "no");

				_mpv_set_option_string(mpv, "config", "yes");
				_mpv_set_option_string(mpv, "config-dir", path.c_str());

				if (cfg_mpv_native_logging.get())
				{
					path.add_filename("mpv.log");
					_mpv_set_option_string(mpv, "log-file", path.c_str());
				}

				// no display for music
				_mpv_set_option_string(mpv, "audio-display", "no");

				// everything syncs to foobar
				_mpv_set_option_string(mpv, "video-sync", "audio");
				_mpv_set_option_string(mpv, "untimed", "no");

				// seek fast
				_mpv_set_option_string(mpv, "hr-seek-framedrop", "yes");

				// foobar plays the audio
				_mpv_set_option_string(mpv, "audio", "no");

				// start timing immediately to keep in sync
				_mpv_set_option_string(mpv, "no-initial-audio-sync", "yes");

				// keep the renderer initialised
				_mpv_set_option_string(mpv, "force-window", "yes");
				_mpv_set_option_string(mpv, "idle", "yes");

				if (_mpv_initialize(mpv) != 0)
				{
					_mpv_terminate_destroy(mpv);
					mpv = NULL;
				}
			}

			if (playback_control::get()->is_playing())
			{
				metadb_handle_ptr handle;
				playback_control::get()->get_now_playing(handle);
				play_path(handle->get_path());
			}
		}

		void play_path(const char* metadb_path)
		{
			if (mpv == NULL)
				return;

			std::stringstream time_sstring;
			time_sstring.setf(std::ios::fixed);
			time_sstring.precision(3);
			time_sstring << playback_control::get()->playback_get_position();
			std::string time_string = time_sstring.str();
			_mpv_set_option_string(mpv, "start", time_string.c_str());

			pfc::string8 filename;
			filename.add_filename(metadb_path);
			if (filename.has_prefix("\\file://"))
			{
				filename.remove_chars(0, 8);

				if (filename.is_empty())
					return;

				const char* cmd[] = { "loadfile", filename.c_str(), NULL };
				if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get())
				{
					std::stringstream msg;
					msg << "Mpv: Error loading item '" << filename << "'";
					console::error(msg.str().c_str());
				}
			}
			else if (cfg_mpv_logging.get())
			{
				std::stringstream msg;
				msg << "Mpv: Skipping loading item '" << filename << "' because it is not a local file";
				console::error(msg.str().c_str());
			}

		}

		void stop()
		{
			if (mpv == NULL)
				return;

			if (_mpv_command_string(mpv, "stop") < 0 && cfg_mpv_logging.get())
			{
				console::error("Mpv: Error stopping video");
			}
		}

		void pause(bool state)
		{
			if (mpv == NULL)
				return;

			if (_mpv_set_property_string(mpv, "pause", state ? "yes" : "no") < 0 && cfg_mpv_logging.get())
			{
				console::error("Mpv: Error pausing");
			}
		}

		void seek(double time)
		{
			if (mpv == NULL)
				return;

			std::stringstream time_sstring;
			time_sstring.setf(std::ios::fixed);
			time_sstring.precision(15);
			time_sstring << time;
			std::string time_string = time_sstring.str();
			const char* cmd[] = { "seek", time_string.c_str(), "absolute+exact", NULL };
			if (_mpv_command(mpv, cmd) < 0 && cfg_mpv_logging.get())
			{
				console::error("Mpv: Error seeking");
			}
		}

		void sync()
		{
			if (mpv == NULL)
				return;

			double mpv_time = -1.0;
			if (_mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time) < 0)
				return;

			double desync = playback_control::get()->playback_get_position() - mpv_time;
			double new_speed = 1.0;

			if (abs(desync) > 0.001 * cfg_mpv_hard_sync.get())
			{
				// hard sync
				seek(playback_control::get()->playback_get_position());
				if (cfg_mpv_logging.get())
				{
					console::info("Mpv: A/V sync");
				}
			}
			else
			{
				// soft sync
				if (abs(desync) > 0.001 * cfg_mpv_max_drift.get())
				{
					// aim to correct mpv internal timer in 1 second, then let mpv catch up the video
					new_speed = min(max(1.0 + desync, 0.01), 100.0);
				}
			}

			if (cfg_mpv_logging.get())
			{
				std::stringstream msg;
				msg.setf(std::ios::fixed);
				msg.setf(std::ios::showpos);
				msg.precision(10);
				msg << "Mpv: Video offset " << desync << "; setting mpv speed to " << new_speed;

				console::info(msg.str().c_str());
			}

			if (_mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &new_speed) < 0 && cfg_mpv_logging.get())
			{
				console::error("Mpv: Error setting speed");
			}
		}


	protected:
		const ui_element_instance_callback_ptr m_callback;
	};
	class ui_element_mpvimpl : public ui_element_impl<CMpvWindow> {};
	static service_factory_single_t<ui_element_mpvimpl> g_ui_element_mpvimpl_factory;
} // namespace

