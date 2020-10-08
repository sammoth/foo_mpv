#include "stdafx.h"
// PCH ^

#include <list>

#include "../helpers/atl-misc.h"
#include "artwork_protocol.h"
#include "foobar2000-sdk/libPPUI/Controls.h"
#include "menu_utils.h"
#include "mpv_container.h"
#include "mpv_player.h"
#include "preferences.h"
#include "resource.h"

namespace mpv {
const unsigned pref_page_header_font_size = 15;
const char* default_mpv_conf = "# advanced config here\r\n";

const char* default_input_conf =
    "WHEEL_UP script-message foobar seek backward\r\n"
    "WHEEL_DOWN script-message foobar seek forward\r\n"
    "MOUSE_BTN0_DBL script-message foobar fullscreen\r\n"
    "f script-message foobar fullscreen\r\n";

class default_config_create : public initquit {
 public:
  void on_init() override {
    pfc::string_formatter path;

    filesystem::g_get_native_path(core_api::get_profile_path(), path);
    path.add_filename("mpv");
    try {
      filesystem::g_create_directory(path, fb2k::noAbort);
    } catch (exception_io_already_exists) {
    } catch (...) {
      FB2K_console_formatter()
          << "mpv: Error creating mpv configuration directory";
      return;
    }

    filesystem::g_get_native_path(core_api::get_profile_path(), path);
    path.add_filename("mpv");
    path.add_filename("mpv.conf");
    if (!filesystem::g_exists(path.c_str(), fb2k::noAbort)) {
      try {
        file::ptr config;
        filesystem::g_open(config, path, filesystem::open_mode_write_new,
                           fb2k::noAbort);
        config->write_string_raw(default_mpv_conf, fb2k::noAbort);
      } catch (...) {
        FB2K_console_formatter() << "mpv: Error creating default input.conf";
      }
    }

    filesystem::g_get_native_path(core_api::get_profile_path(), path);
    path.add_filename("mpv");
    path.add_filename("input.conf");
    if (!filesystem::g_exists(path.c_str(), fb2k::noAbort)) {
      try {
        file::ptr config;
        filesystem::g_open(config, path, filesystem::open_mode_write_new,
                           fb2k::noAbort);
        config->write_string_raw(default_input_conf, fb2k::noAbort);
      } catch (...) {
        FB2K_console_formatter() << "mpv: Error creating default input.conf";
      }
    }
  }
};

static initquit_factory_t<default_config_create> g_default_config_create;

static const GUID guid_cfg_bg_color = {
    0xb62c3ef, 0x3c6e, 0x4620, {0xbf, 0xa2, 0x24, 0xa, 0x5e, 0xdd, 0xbc, 0x4b}};
static const GUID guid_cfg_popup_titleformat = {
    0x811a9299,
    0x833d,
    0x4d38,
    {0xb3, 0xee, 0x89, 0x19, 0x8d, 0xed, 0x20, 0x77}};
static const GUID guid_cfg_black_fullscreen = {
    0x2e633cfd,
    0xbb88,
    0x4e69,
    {0xa0, 0xe4, 0x7f, 0x23, 0x2a, 0xdc, 0x5a, 0xd6}};
static const GUID guid_cfg_stop_hidden = {
    0x9de7e631,
    0x64f8,
    0x4047,
    {0x88, 0x39, 0x8f, 0x4a, 0x50, 0xa0, 0xb7, 0x2f}};
static const GUID guid_cfg_branch = {
    0xa8d3b2ca,
    0xa9a,
    0x4efc,
    {0xa4, 0x33, 0x32, 0x4d, 0x76, 0xcc, 0x8a, 0x33}};
static const GUID guid_cfg_max_drift = {
    0x69ee4b45,
    0x9688,
    0x45e8,
    {0x89, 0x44, 0x8a, 0xb0, 0x92, 0xd6, 0xf3, 0xf8}};
static const GUID guid_cfg_hard_sync_interval = {
    0xa095f426,
    0x3df7,
    0x434e,
    {0x91, 0x13, 0x19, 0x1a, 0x1b, 0x5f, 0xc1, 0xe5}};
static const GUID guid_cfg_hard_sync = {
    0xeffb3f43,
    0x60a0,
    0x4a2f,
    {0xbd, 0x78, 0x27, 0x43, 0xa1, 0x6d, 0xd2, 0xbb}};
static const GUID guid_cfg_logging = {
    0x8b74d741,
    0x232a,
    0x46d5,
    {0xa7, 0xee, 0x4, 0x89, 0xb1, 0x47, 0x43, 0xf0}};
static const GUID guid_cfg_native_logging = {
    0x3411741c,
    0x239,
    0x441d,
    {0x8a, 0x8e, 0x99, 0x83, 0x2a, 0xda, 0xe7, 0xd0}};
static const GUID guid_cfg_seek_seconds = {
    0xa9f41576,
    0xaf69,
    0x4579,
    {0xa7, 0x45, 0x93, 0x35, 0xe7, 0xcc, 0x76, 0xd1}};
static const GUID guid_cfg_video_enabled = {
    0xe3a285f2,
    0x6804,
    0x4291,
    {0xa6, 0x8d, 0xb6, 0xac, 0x41, 0x89, 0x8c, 0x1d}};
static const GUID guid_cfg_panel_metric = {
    0xe8f4e209,
    0x9583,
    0x496d,
    {0xb6, 0xa3, 0x8e, 0xed, 0x10, 0x86, 0xae, 0x44}};

static const GUID guid_cfg_thumbnails = {
    0xa06a6a79,
    0xff21,
    0x453f,
    {0xa7, 0x6c, 0xbf, 0xf0, 0x3b, 0x3f, 0xee, 0x4b}};
static const GUID guid_cfg_thum_cover_type = {
    0xe2525d22,
    0xabcc,
    0x4ed4,
    {0xa8, 0x9b, 0x16, 0x18, 0xa5, 0xfc, 0x6, 0x21}};
static const GUID guid_cfg_item_in_group = {
    0x9be7baa, 0x9e8f, 0x47cb, {0xb4, 0xc9, 0x8c, 0xcf, 0x6, 0x45, 0x2a, 0x1}};
static const GUID guid_cfg_group_override = {
    0xec868147,
    0x4e80,
    0x4703,
    {0x86, 0x6f, 0xe4, 0x18, 0x3a, 0x89, 0x5b, 0x24}};
static const GUID guid_cfg_thumb_size = {
    0xc181478a,
    0x2d69,
    0x4933,
    {0x96, 0x7, 0x5c, 0x52, 0x11, 0x55, 0xa5, 0xb6}};
static const GUID guid_cfg_thumb_seek = {
    0x6e455d58,
    0xe11a,
    0x4158,
    {0x92, 0x4c, 0x29, 0x50, 0xa4, 0x70, 0xb9, 0xe7}};
static const GUID guid_cfg_thumb_histogram = {
    0xbf7cab8,
    0x1365,
    0x4f27,
    {0xbd, 0x14, 0x22, 0xc6, 0x99, 0x7c, 0x22, 0x8e}};
static const GUID guid_cfg_thumb_cache_size = {
    0x5163cef5,
    0xd926,
    0x4ce9,
    {0x88, 0x18, 0xee, 0xa9, 0x19, 0x6f, 0xe7, 0x73}};
static const GUID guid_cfg_filter = {
    0xeedb191b,
    0xddfb,
    0x4acc,
    {0x86, 0xe1, 0xf6, 0x43, 0xcf, 0x34, 0x4e, 0x42}};
static const GUID guid_cfg_pattern = {
    0xa7dd375a,
    0x40fd,
    0x488b,
    {0xbb, 0x35, 0xdb, 0x6, 0xf5, 0xf6, 0x12, 0xb7}};
static const GUID guid_cfg_cache_format = {
    0xfc82870,
    0x6b5f,
    0x49a7,
    {0x8c, 0xa1, 0xca, 0x98, 0xdd, 0x5b, 0xa9, 0x20}};
static const GUID guid_cfg_artwork = {
    0xe0ce0090,
    0x762f,
    0x4cb4,
    {0xad, 0xef, 0xfe, 0xd0, 0xae, 0xf1, 0xd1, 0x93}};
static const GUID guid_cfg_artwork_type = {
    0xf305ac3b,
    0x9af1,
    0x449f,
    {0xb7, 0x95, 0x80, 0xd7, 0x11, 0x31, 0x18, 0xdf}};
static const GUID guid_cfg_osc = {
    0xf00436f5,
    0x5ceb,
    0x480b,
    {0x9d, 0xd6, 0x87, 0x89, 0x45, 0xc0, 0x98, 0x48}};
static const GUID guid_cfg_osc_layout = {
    0x168414ff,
    0xf235,
    0x48b1,
    {0x9d, 0x4f, 0x37, 0x7b, 0xe9, 0xbd, 0x81, 0xc5}};
static const GUID guid_cfg_osc_seekbarstyle = {
    0x89a630d5,
    0x4d5f,
    0x49da,
    {0x85, 0x96, 0x8f, 0xba, 0xba, 0x70, 0x28, 0xf3}};
static const GUID guid_cfg_osc_transparency = {
    0xed8f6b2f,
    0xd632,
    0x4ebb,
    {0x95, 0xe0, 0xd9, 0x28, 0xb3, 0xec, 0x28, 0xcf}};
static const GUID guid_cfg_osc_deadzone = {
    0x17a83aea,
    0x63eb,
    0x44ef,
    {0x89, 0xf7, 0xe6, 0x3e, 0xb3, 0x1f, 0x27, 0x6}};
static const GUID guid_cfg_osc_timeout = {
    0x72956f83,
    0x98b0,
    0x4893,
    {0x94, 0x7c, 0x9c, 0xb6, 0xc3, 0xb4, 0x8c, 0xd0}};
static const GUID guid_cfg_osc_scalewindowed = {
    0x9d193141,
    0xc712,
    0x4a7b,
    {0x97, 0x16, 0x7d, 0xa1, 0x9c, 0x66, 0x0, 0xe3}};
static const GUID guid_cfg_osc_scalefullscreen = {
    0x42e27062,
    0x5b5d,
    0x4212,
    {0xb3, 0x28, 0x4c, 0x12, 0xbe, 0xca, 0x50, 0xed}};
static const GUID guid_cfg_osc_scalewithvideo = {
    0x59c014a8,
    0x5d1,
    0x4498,
    {0xb1, 0x1d, 0x56, 0x9d, 0xf8, 0x20, 0xfb, 0x78}};
static const GUID guid_cfg_osc_fadeduration = {
    0x8ba53c19,
    0x9f7a,
    0x493f,
    {0x8c, 0x9, 0xdb, 0x19, 0x82, 0x19, 0x1b, 0x34}};
static const GUID guid_cfg_hwdec = {
    0x9eed4fd,
    0x6366,
    0x4bcd,
    {0xb8, 0xa9, 0x2e, 0xaa, 0xf7, 0x58, 0x22, 0xf0}};
static const GUID guid_cfg_deint = {
    0x454c3cd1,
    0x7d6f,
    0x4069,
    {0xb4, 0xbd, 0x1d, 0x70, 0x27, 0x22, 0xa7, 0x96}};
static const GUID guid_cfg_latency = {
    0x11e1fcbd,
    0x859c,
    0x4cab,
    {0xb0, 0x92, 0x9f, 0x2f, 0x0, 0x2b, 0x66, 0x5f}};
static const GUID guid_cfg_gpuhq = {
    0x9cb1a3fe,
    0xc926,
    0x4c07,
    {0x8e, 0x3b, 0x22, 0xb7, 0xbb, 0xea, 0x0, 0xfd}};

cfg_bool cfg_video_enabled(guid_cfg_video_enabled, true);

cfg_uint cfg_bg_color(guid_cfg_bg_color, 0);
cfg_bool cfg_black_fullscreen(guid_cfg_black_fullscreen, true);
cfg_bool cfg_stop_hidden(guid_cfg_stop_hidden, true);
cfg_uint cfg_panel_metric(guid_cfg_panel_metric, 0);

cfg_bool cfg_hwdec(guid_cfg_hwdec, false);
cfg_bool cfg_deint(guid_cfg_deint, true);
cfg_bool cfg_latency(guid_cfg_latency, false);
cfg_bool cfg_gpuhq(guid_cfg_gpuhq, false);

static const char* cfg_popup_titleformat_default =
    "%title% - %artist%[' ('%album%')']";
static cfg_string cfg_popup_titleformat(guid_cfg_popup_titleformat,
                                        cfg_popup_titleformat_default);

cfg_bool cfg_thumbs(guid_cfg_thumbnails, true);
cfg_uint cfg_thumb_cover_type(guid_cfg_thum_cover_type, 0);
cfg_bool cfg_thumb_group_longest(guid_cfg_item_in_group, false);
cfg_bool cfg_thumb_group_override(guid_cfg_group_override, true);
cfg_uint cfg_thumb_size(guid_cfg_thumb_size, 2);
cfg_uint cfg_thumb_seek(guid_cfg_thumb_seek, 30);
cfg_bool cfg_thumb_histogram(guid_cfg_thumb_histogram, false);
cfg_uint cfg_thumb_cache_size(guid_cfg_thumb_cache_size, 0);
cfg_uint cfg_thumb_cache_format(guid_cfg_cache_format, 0);
cfg_bool cfg_thumb_filter(guid_cfg_filter, false);

cfg_bool cfg_artwork(guid_cfg_artwork, true);
cfg_uint cfg_artwork_type(guid_cfg_artwork_type, 0);

cfg_bool cfg_osc(guid_cfg_osc, true);
cfg_bool cfg_osc_scalewithvideo(guid_cfg_osc_scalewithvideo, true);
cfg_uint cfg_osc_layout(guid_cfg_osc_layout, 0);
cfg_uint cfg_osc_seekbarstyle(guid_cfg_osc_seekbarstyle, 0);
cfg_uint cfg_osc_scalewindowed(guid_cfg_osc_scalewindowed, 100);
cfg_uint cfg_osc_scalefullscreen(guid_cfg_osc_scalefullscreen, 100);
cfg_uint cfg_osc_transparency(guid_cfg_osc_transparency, 30);
cfg_uint cfg_osc_fadeduration(guid_cfg_osc_fadeduration, 200);
cfg_uint cfg_osc_timeout(guid_cfg_osc_timeout, 500);
cfg_uint cfg_osc_deadzone(guid_cfg_osc_deadzone, 50);

static const char* cfg_thumb_pattern_default =
    "\"$ext(%filename_ext%)\" IS mkv";
static cfg_string cfg_thumb_pattern(guid_cfg_pattern,
                                    cfg_thumb_pattern_default);

static advconfig_branch_factory g_mpv_branch(
    "mpv", guid_cfg_branch, advconfig_branch::guid_branch_playback, 0);

advconfig_integer_factory cfg_max_drift("Permitted timing drift (ms)",
                                        guid_cfg_max_drift, guid_cfg_branch, 0,
                                        20, 0, 1000, 0);
advconfig_integer_factory cfg_hard_sync_threshold("Hard sync threshold (ms)",
                                                  guid_cfg_hard_sync,
                                                  guid_cfg_branch, 0, 3000, 0,
                                                  10000, 0);
advconfig_integer_factory cfg_hard_sync_interval(
    "Minimum time between hard syncs (seconds)", guid_cfg_hard_sync_interval,
    guid_cfg_branch, 0, 10, 0, 30, 0);

advconfig_integer_factory cfg_seek_seconds("Manual seek distance (seconds)",
                                           guid_cfg_seek_seconds,
                                           guid_cfg_branch, 0, 5, 0, 1000, 0);

advconfig_checkbox_factory cfg_logging("Enable verbose console logging",
                                       guid_cfg_logging, guid_cfg_branch, 0,
                                       false);
advconfig_checkbox_factory cfg_mpv_logfile("Enable mpv log file",
                                           guid_cfg_native_logging,
                                           guid_cfg_branch, 0, false);

static titleformat_object::ptr popup_titleformat_script;
static search_filter::ptr thumb_filter;

void format_player_title(pfc::string8& s, metadb_handle_ptr item) {
  if (popup_titleformat_script.is_empty()) {
    static_api_ptr_t<titleformat_compiler>()->compile_safe(
        popup_titleformat_script, cfg_popup_titleformat);
  }

  if (item.is_valid()) {
    item->format_title(NULL, s, popup_titleformat_script, NULL);
  }
}

bool test_thumb_pattern(metadb_handle_ptr metadb) {
  if (!cfg_thumb_filter) return true;
  if (thumb_filter.is_empty()) {
    try {
      thumb_filter =
          static_api_ptr_t<search_filter_manager>()->create(cfg_thumb_pattern);
    } catch (std::exception e) {
      return false;
    }
  }

  metadb_handle_list in;
  in.add_item(metadb);
  bool out;
  thumb_filter->test_multi(in, &out);
  return out;
}

class CMpvPlayerPreferences : public CDialogImpl<CMpvPlayerPreferences>,
                              public preferences_page_instance {
 public:
  CMpvPlayerPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback), button_brush(CreateSolidBrush(cfg_bg_color)) {}
  ~CMpvPlayerPreferences() {
    if (sep_font != NULL) DeleteObject(sep_font);
  }

  enum { IDD = IDD_MPV_PREFS };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMpvPlayerPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  MSG_WM_CTLCOLORBTN(on_color_button);
  MSG_WM_HSCROLL(OnScroll);
  COMMAND_HANDLER_EX(IDC_BUTTON_BG, BN_CLICKED, OnBgClick);
  COMMAND_HANDLER_EX(IDC_CHECK_ARTWORK, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_FSBG, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_STOP, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_HWDEC, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_DEINT, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_LATENCY, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_GPUHQ, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_EDIT_POPUP, EN_CHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_PANELMETRIC, CBN_SELCHANGE, OnEditChange);
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnBgClick(UINT, int, CWindow);
  void OnEditChange(UINT, int, CWindow);
  void OnScroll(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  CBrush button_brush;
  HBRUSH on_color_button(HDC wp, HWND lp);
  bool dirty = false;

  void set_controls_enabled();

  const preferences_page_callback::ptr m_callback;

  COLORREF bg_col = 0;

  HFONT sep_font = NULL;
};

HBRUSH CMpvPlayerPreferences::on_color_button(HDC wp, HWND lp) {
  if (lp == GetDlgItem(IDC_BUTTON_BG)) {
    return button_brush;
  }
  return NULL;
}

BOOL CMpvPlayerPreferences::OnInitDialog(CWindow, LPARAM) {
  UINT header_size = pref_page_header_font_size;

  HDC hdc = GetDC();
  if (hdc != NULL) {
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    header_size = (header_size * dpi) / 72;
  }
  sep_font =
      CreateFont(header_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));

  ((CStatic)GetDlgItem(IDC_STATIC_SECTION1)).SetFont(sep_font);
  ((CStatic)GetDlgItem(IDC_STATIC_SECTION5)).SetFont(sep_font);

  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_popup_titleformat);

  bg_col = cfg_bg_color.get_value();
  button_brush = CreateSolidBrush(bg_col);

  CheckDlgButton(IDC_CHECK_ARTWORK, cfg_artwork);
  CheckDlgButton(IDC_CHECK_FSBG, cfg_black_fullscreen);
  CheckDlgButton(IDC_CHECK_STOP, cfg_stop_hidden);
  CheckDlgButton(IDC_CHECK_DEINT, cfg_deint);
  CheckDlgButton(IDC_CHECK_LATENCY, cfg_latency);
  CheckDlgButton(IDC_CHECK_HWDEC, cfg_hwdec);
  CheckDlgButton(IDC_CHECK_GPUHQ, cfg_gpuhq);

  CComboBox combo_panelmetric = (CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC);
  combo_panelmetric.AddString(L"Area");
  combo_panelmetric.AddString(L"Width");
  combo_panelmetric.AddString(L"Height");
  combo_panelmetric.SetCurSel(cfg_panel_metric);

  set_controls_enabled();

  dirty = false;

  return FALSE;
}

void CMpvPlayerPreferences::OnBgClick(UINT, int, CWindow) {
  CHOOSECOLOR cc = {};
  static COLORREF acrCustClr[16];
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = get_wnd();
  cc.lpCustColors = (LPDWORD)acrCustClr;
  cc.rgbResult = bg_col;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;

  if (ChooseColor(&cc) == TRUE) {
    bg_col = cc.rgbResult;
    button_brush = CreateSolidBrush(bg_col);
    dirty = true;
    OnChanged();
    Invalidate();
  }
}

void CMpvPlayerPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

void CMpvPlayerPreferences::OnScroll(UINT, int, CWindow) {
  dirty = true;
  m_callback->on_state_changed();
}

t_uint32 CMpvPlayerPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvPlayerPreferences::reset() {
  bg_col = 0;
  button_brush = CreateSolidBrush(bg_col);

  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_popup_titleformat_default);

  CheckDlgButton(IDC_CHECK_ARTWORK, true);
  CheckDlgButton(IDC_CHECK_FSBG, true);
  CheckDlgButton(IDC_CHECK_STOP, true);
  CheckDlgButton(IDC_CHECK_DEINT, true);
  CheckDlgButton(IDC_CHECK_HWDEC, false);
  CheckDlgButton(IDC_CHECK_LATENCY, false);
  CheckDlgButton(IDC_CHECK_GPUHQ, false);

  ((CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC)).SetCurSel(0);

  dirty = true;
  OnChanged();
}

void CMpvPlayerPreferences::apply() {
  cfg_bg_color = bg_col;

  cfg_artwork = IsDlgButtonChecked(IDC_CHECK_ARTWORK);
  cfg_black_fullscreen = IsDlgButtonChecked(IDC_CHECK_FSBG);
  cfg_stop_hidden = IsDlgButtonChecked(IDC_CHECK_STOP);
  cfg_hwdec = IsDlgButtonChecked(IDC_CHECK_HWDEC);
  cfg_deint = IsDlgButtonChecked(IDC_CHECK_DEINT);
  cfg_latency = IsDlgButtonChecked(IDC_CHECK_LATENCY);
  cfg_gpuhq = IsDlgButtonChecked(IDC_CHECK_GPUHQ);

  pfc::string format = uGetDlgItemText(m_hWnd, IDC_EDIT_POPUP);
  cfg_popup_titleformat.reset();
  cfg_popup_titleformat.set_string(format.get_ptr());

  static_api_ptr_t<titleformat_compiler>()->compile_safe(
      popup_titleformat_script, cfg_popup_titleformat);

  cfg_panel_metric =
      ((CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC)).GetCurSel();

  mpv_container::invalidate_all_containers();
  mpv_player::restart();
  dirty = false;
  OnChanged();
  reload_artwork();
}

bool CMpvPlayerPreferences::HasChanged() { return dirty; }

void CMpvPlayerPreferences::set_controls_enabled() {}

void CMpvPlayerPreferences::OnChanged() {
  m_callback->on_state_changed();
  set_controls_enabled();
}

class CMpvThumbnailPreferences : public CDialogImpl<CMpvThumbnailPreferences>,
                                 public preferences_page_instance {
 public:
  CMpvThumbnailPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback) {}
  ~CMpvThumbnailPreferences() {
    if (sep_font != NULL) DeleteObject(sep_font);
  }

  enum { IDD = IDD_MPV_PREFS1 };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMpvThumbnailPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  MSG_WM_HSCROLL(OnScroll);
  COMMAND_HANDLER_EX(IDC_EDIT_PATTERN, EN_CHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_THUMBNAILS, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_RADIO_FIRSTINGROUP, BN_CLICKED, OnEditChange)
  COMMAND_HANDLER_EX(IDC_RADIO_LONGESTINGROUP, BN_CLICKED, OnEditChange)
  COMMAND_HANDLER_EX(IDC_CHECK_HISTOGRAM, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_COVERTYPE, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_GROUPOVERRIDE, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_FILTER, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_FORMAT, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_CACHESIZE, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_THUMBSIZE, CBN_SELCHANGE, OnEditChange);
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnEditChange(UINT, int, CWindow);
  void OnScroll(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  bool dirty = false;

  void set_controls_enabled();

  const preferences_page_callback::ptr m_callback;

  HFONT sep_font = NULL;
};

BOOL CMpvThumbnailPreferences::OnInitDialog(CWindow, LPARAM) {
  UINT header_size = pref_page_header_font_size;

  HDC hdc = GetDC();
  if (hdc != NULL) {
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    header_size = (header_size * dpi) / 72;
  }
  sep_font =
      CreateFont(header_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));

  ((CStatic)GetDlgItem(IDC_STATIC_SECTION4)).SetFont(sep_font);
  ((CStatic)GetDlgItem(IDC_STATIC_SECTION3)).SetFont(sep_font);

  uSetDlgItemText(m_hWnd, IDC_EDIT_PATTERN, cfg_thumb_pattern);

  CheckDlgButton(IDC_CHECK_THUMBNAILS, cfg_thumbs);
  CheckDlgButton(IDC_CHECK_HISTOGRAM, cfg_thumb_histogram);
  CheckDlgButton(IDC_CHECK_FILTER, cfg_thumb_filter);
  CheckDlgButton(IDC_CHECK_GROUPOVERRIDE, cfg_thumb_group_override);
  CheckDlgButton(IDC_RADIO_LONGESTINGROUP, cfg_thumb_group_longest);
  CheckDlgButton(IDC_RADIO_FIRSTINGROUP, !cfg_thumb_group_longest);

  CComboBox combo_covertype = (CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE);
  combo_covertype.AddString(L"Front");
  combo_covertype.AddString(L"Back");
  combo_covertype.AddString(L"Disc");
  combo_covertype.AddString(L"Artist");
  combo_covertype.AddString(L"All");
  combo_covertype.SetCurSel(cfg_thumb_cover_type);

  CComboBox combo_thumbsize = (CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE);
  combo_thumbsize.AddString(L"200px");
  combo_thumbsize.AddString(L"400px");
  combo_thumbsize.AddString(L"600px");
  combo_thumbsize.AddString(L"1000px");
  combo_thumbsize.AddString(L"Original");
  combo_thumbsize.SetCurSel(cfg_thumb_size);

  CComboBox combo_cachesize = (CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE);
  combo_cachesize.AddString(L"200MB");
  combo_cachesize.AddString(L"500MB");
  combo_cachesize.AddString(L"1GB");
  combo_cachesize.AddString(L"2GB");
  combo_cachesize.AddString(L"Unlimited");
  combo_cachesize.SetCurSel(cfg_thumb_cache_size);

  CComboBox combo_cacheformat = (CComboBox)uGetDlgItem(IDC_COMBO_FORMAT);
  combo_cacheformat.AddString(L"JPEG");
  combo_cacheformat.AddString(L"PNG");
  combo_cacheformat.SetCurSel(cfg_thumb_cache_format);

  CTrackBarCtrl slider_seek = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_SEEK);
  slider_seek.SetRangeMin(1);
  slider_seek.SetRangeMax(90);
  slider_seek.SetPos(cfg_thumb_seek);

  set_controls_enabled();

  dirty = false;

  return FALSE;
}

void CMpvThumbnailPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

void CMpvThumbnailPreferences::OnScroll(UINT, int, CWindow) {
  dirty = true;
  m_callback->on_state_changed();
}

t_uint32 CMpvThumbnailPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvThumbnailPreferences::reset() {
  uSetDlgItemText(m_hWnd, IDC_EDIT_PATTERN, cfg_thumb_pattern_default);

  CheckDlgButton(IDC_CHECK_THUMBNAILS, true);
  CheckDlgButton(IDC_CHECK_FILTER, false);
  CheckDlgButton(IDC_CHECK_HISTOGRAM, false);
  CheckDlgButton(IDC_CHECK_GROUPOVERRIDE, true);
  CheckDlgButton(IDC_RADIO_LONGESTINGROUP, false);
  CheckDlgButton(IDC_RADIO_FIRSTINGROUP, true);

  ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).SetCurSel(2);
  ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).SetCurSel(0);

  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_SEEK)).SetPos(30);

  dirty = true;
  OnChanged();
}

void CMpvThumbnailPreferences::apply() {
  pfc::string format = uGetDlgItemText(m_hWnd, IDC_EDIT_PATTERN);
  cfg_thumb_pattern.reset();
  cfg_thumb_pattern.set_string(format.get_ptr());

  try {
    thumb_filter =
        static_api_ptr_t<search_filter_manager>()->create(cfg_thumb_pattern);
  } catch (std::exception ex) {
    thumb_filter.reset();
  }

  cfg_thumbs = IsDlgButtonChecked(IDC_CHECK_THUMBNAILS);
  cfg_thumb_filter = IsDlgButtonChecked(IDC_CHECK_FILTER);
  cfg_thumb_histogram = IsDlgButtonChecked(IDC_CHECK_HISTOGRAM);
  cfg_thumb_group_longest = IsDlgButtonChecked(IDC_RADIO_LONGESTINGROUP);
  cfg_thumb_group_override = IsDlgButtonChecked(IDC_CHECK_GROUPOVERRIDE);

  cfg_thumb_cover_type =
      ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).GetCurSel();
  cfg_thumb_size = ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).GetCurSel();
  cfg_thumb_cache_size =
      ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).GetCurSel();
  cfg_thumb_cache_format =
      ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).GetCurSel();

  cfg_thumb_seek = ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_SEEK)).GetPos();

  mpv_container::invalidate_all_containers();
  dirty = false;
  OnChanged();
  reload_artwork();
}

bool CMpvThumbnailPreferences::HasChanged() { return dirty; }

void CMpvThumbnailPreferences::set_controls_enabled() {
  bool thumbs = IsDlgButtonChecked(IDC_CHECK_THUMBNAILS);
  bool pattern = IsDlgButtonChecked(IDC_CHECK_FILTER);
  bool automatic = IsDlgButtonChecked(IDC_CHECK_HISTOGRAM);

  ((CComboBox)uGetDlgItem(IDC_EDIT_PATTERN)).EnableWindow(thumbs && pattern);
  ((CComboBox)uGetDlgItem(IDC_CHECK_FILTER)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_RADIO_LONGESTINGROUP)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_RADIO_FIRSTINGROUP)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_CHECK_HISTOGRAM)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_CHECK_GROUPOVERRIDE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_SLIDER_SEEK)).EnableWindow(thumbs && !automatic);
}

void CMpvThumbnailPreferences::OnChanged() {
  m_callback->on_state_changed();
  set_controls_enabled();
}

class CMpvOscPreferences : public CDialogImpl<CMpvOscPreferences>,
                           public preferences_page_instance {
 public:
  CMpvOscPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback) {}
  ~CMpvOscPreferences() {
    if (sep_font != NULL) DeleteObject(sep_font);
  }

  enum { IDD = IDD_MPV_PREFS2 };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMpvOscPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  MSG_WM_HSCROLL(OnScroll);
  COMMAND_HANDLER_EX(IDC_CHECK_OSC, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_OSC_SCALEWITHVIDEO, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_OSC_LAYOUT, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_OSC_SEEKBARSTYLE, CBN_SELCHANGE, OnEditChange);
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnEditChange(UINT, int, CWindow);
  void OnScroll(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  bool dirty = false;

  const preferences_page_callback::ptr m_callback;

  HFONT sep_font = NULL;
};

BOOL CMpvOscPreferences::OnInitDialog(CWindow, LPARAM) {
  UINT header_size = pref_page_header_font_size;

  HDC hdc = GetDC();
  if (hdc != NULL) {
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    header_size = (header_size * dpi) / 72;
  }
  sep_font =
      CreateFont(header_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));

  ((CStatic)GetDlgItem(IDC_STATIC_SECTION2)).SetFont(sep_font);

  CheckDlgButton(IDC_CHECK_OSC, cfg_osc);
  CheckDlgButton(IDC_CHECK_OSC_SCALEWITHVIDEO, cfg_osc_scalewithvideo);

  CComboBox combo = (CComboBox)uGetDlgItem(IDC_COMBO_OSC_LAYOUT);
  combo.AddString(L"Bottom bar");
  combo.AddString(L"Top bar");
  combo.AddString(L"Box");
  combo.AddString(L"Slim box");
  combo.SetCurSel(cfg_osc_layout);

  combo = (CComboBox)uGetDlgItem(IDC_COMBO_OSC_SEEKBARSTYLE);
  combo.AddString(L"Bar");
  combo.AddString(L"Diamond");
  combo.AddString(L"Knob");
  combo.SetCurSel(cfg_osc_seekbarstyle);

  CTrackBarCtrl slider =
      (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TRANSPARENCY);
  slider.SetRangeMin(0);
  slider.SetRangeMax(100);
  slider.SetPos(cfg_osc_transparency);

  slider = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_WINDOW);
  slider.SetRangeMin(0);
  slider.SetRangeMax(300);
  slider.SetPos(cfg_osc_scalewindowed);

  slider = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_FULLSCREEN);
  slider.SetRangeMin(0);
  slider.SetRangeMax(300);
  slider.SetPos(cfg_osc_scalefullscreen);

  slider = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TIMEOUT);
  slider.SetRangeMin(0);
  slider.SetRangeMax(5000);
  slider.SetPos(cfg_osc_timeout);

  slider = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_FADE);
  slider.SetRangeMin(0);
  slider.SetRangeMax(1000);
  slider.SetPos(cfg_osc_fadeduration);

  slider = (CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_DEADZONE);
  slider.SetRangeMin(0);
  slider.SetRangeMax(100);
  slider.SetPos(cfg_osc_deadzone);

  dirty = false;

  return FALSE;
}

void CMpvOscPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

void CMpvOscPreferences::OnScroll(UINT, int, CWindow) {
  dirty = true;
  m_callback->on_state_changed();
}

t_uint32 CMpvOscPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvOscPreferences::reset() {
  CheckDlgButton(IDC_CHECK_OSC, true);
  CheckDlgButton(IDC_CHECK_OSC_SCALEWITHVIDEO, true);

  ((CComboBox)uGetDlgItem(IDC_COMBO_OSC_LAYOUT)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_OSC_SEEKBARSTYLE)).SetCurSel(0);

  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_DEADZONE)).SetPos(50);
  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_FADE)).SetPos(200);
  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TIMEOUT)).SetPos(500);
  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_WINDOW)).SetPos(100);
  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_FULLSCREEN)).SetPos(100);
  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TRANSPARENCY)).SetPos(30);

  dirty = true;
  OnChanged();
}

void CMpvOscPreferences::apply() {
  cfg_osc = IsDlgButtonChecked(IDC_CHECK_OSC);

  cfg_osc_scalewithvideo = IsDlgButtonChecked(IDC_CHECK_OSC_SCALEWITHVIDEO);

  cfg_osc_seekbarstyle =
      ((CComboBox)uGetDlgItem(IDC_COMBO_OSC_SEEKBARSTYLE)).GetCurSel();
  cfg_osc_layout = ((CComboBox)uGetDlgItem(IDC_COMBO_OSC_LAYOUT)).GetCurSel();

  cfg_osc_transparency =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TRANSPARENCY)).GetPos();
  cfg_osc_fadeduration =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_FADE)).GetPos();
  cfg_osc_deadzone =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_DEADZONE)).GetPos();
  cfg_osc_scalefullscreen =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_FULLSCREEN)).GetPos();
  cfg_osc_scalewindowed =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_SCALE_WINDOW)).GetPos();
  cfg_osc_timeout =
      ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_OSC_TIMEOUT)).GetPos();

  mpv_player::restart();
  dirty = false;
  OnChanged();
}

bool CMpvOscPreferences::HasChanged() { return dirty; }

void CMpvOscPreferences::OnChanged() { m_callback->on_state_changed(); }

class CMpvConfPreferences : public CDialogImpl<CMpvConfPreferences>,
                            public preferences_page_instance {
 public:
  CMpvConfPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback) {}
  ~CMpvConfPreferences() {
    if (edit_font != NULL) DeleteObject(edit_font);
    if (sep_font != NULL) DeleteObject(sep_font);
  }

  enum { IDD = IDD_MPV_PREFS3 };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMpvConfPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  COMMAND_HANDLER_EX(IDC_EDIT1, EN_CHANGE, OnEditChange);
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnEditChange(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  bool dirty = false;

  const preferences_page_callback::ptr m_callback;

  HFONT edit_font = NULL;
  HFONT sep_font = NULL;
};

BOOL CMpvConfPreferences::OnInitDialog(CWindow, LPARAM) {
  UINT header_size = pref_page_header_font_size;
  UINT text_size = 11;

  HDC hdc = GetDC();
  if (hdc != NULL) {
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    text_size = (text_size * dpi) / 72;
    header_size = (header_size * dpi) / 72;
  }

  edit_font =
      CreateFont(text_size, 0, 0, 0, FW_REGULAR, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, FIXED_PITCH, NULL);
  CEdit edit = ((CEdit)GetDlgItem(IDC_EDIT1));
  edit.SetFont(edit_font);

  sep_font =
      CreateFont(header_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));
  ((CStatic)GetDlgItem(IDC_STATIC_SECTION10)).SetFont(sep_font);

  pfc::string_formatter path;
  filesystem::g_get_native_path(core_api::get_profile_path(), path);
  path.add_filename("mpv");
  path.add_filename("mpv.conf");
  file::ptr config;
  pfc::string8 contents;
  try {
    filesystem::g_open(config, path, filesystem::open_mode_read, fb2k::noAbort);
    config->read_string_raw(contents, fb2k::noAbort);
  } catch (...) {
    contents = "";
  }

  edit.SetLimitText(0);
  uSetWindowText(edit, contents.c_str());

  dirty = false;

  return FALSE;
}

void CMpvConfPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

t_uint32 CMpvConfPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvConfPreferences::reset() {
  dirty = true;
  OnChanged();
}

void CMpvConfPreferences::apply() {
  pfc::string content = uGetWindowText(GetDlgItem(IDC_EDIT1));

  pfc::string_formatter path;
  filesystem::g_get_native_path(core_api::get_profile_path(), path);
  path.add_filename("mpv");
  try {
    filesystem::g_create_directory(path, fb2k::noAbort);
  } catch (exception_io_already_exists) {
  } catch (...) {
    popup_message::g_complain("Error creating mpv configuration directory");
    return;
  }

  path.add_filename("mpv.conf");

  file::ptr config;
  try {
    filesystem::g_open(config, path, filesystem::open_mode_write_new,
                       fb2k::noAbort);
    config->write_string_raw(content.c_str(), fb2k::noAbort);
  } catch (...) {
    popup_message::g_complain("Error writing mpv.conf");
    return;
  }

  mpv_player::restart();

  dirty = false;
  OnChanged();
}

bool CMpvConfPreferences::HasChanged() { return dirty; }

void CMpvConfPreferences::OnChanged() { m_callback->on_state_changed(); }

class CMpvInputPreferences : public CDialogImpl<CMpvInputPreferences>,
                             public preferences_page_instance {
 public:
  CMpvInputPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback) {}
  ~CMpvInputPreferences() {
    if (edit_font != NULL) DeleteObject(edit_font);
    if (sep_font != NULL) DeleteObject(sep_font);
  }

  enum { IDD = IDD_MPV_PREFS4 };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMpvInputPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  COMMAND_HANDLER_EX(IDC_EDIT2, EN_CHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_BUTTON_INPUTHELP, BN_CLICKED, OnHelp);
  COMMAND_HANDLER_EX(IDC_BUTTON_INSERT, BN_CLICKED, OnInsert);
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnEditChange(UINT, int, CWindow);
  void OnHelp(UINT, int, CWindow);
  void OnInsert(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  bool dirty = false;

  std::list<pfc::string8> context_commands;

  const preferences_page_callback::ptr m_callback;

  HFONT edit_font = NULL;
  HFONT sep_font = NULL;
};

void CMpvInputPreferences::OnInsert(UINT, int, CWindow) {
  pfc::string8 text;

  for (auto& item : menu_utils::get_contextmenu_items()) {
    text << item.name << "\n";
  }
  for (auto& item : menu_utils::get_mainmenu_items()) {
    text << item.name << "\n";
  }

  popup_message::g_show(text, "commands");
}

void CMpvInputPreferences::OnHelp(UINT, int, CWindow) {
  popup_message::g_show(
      R"ABC(mpv receives mouse input, except for right clicks, and keyboard input when in popup or fullscreen mode.

The following commands are provided as script-messages with a 'foobar' word prefix for controling playback, and should be used instead of the corresponding mpv commands.

Example usage for binding the f key:
f script-message foobar fullscreen

pause

prev

next

seek backward

seek forward

seek <time>

stop

volup

voldown

fullscreen

context <command>
(runs context menu command on the current playing video or displayed track)

menu <command>
(runs main menu command)

register-titleformat <unique id> <title formatting string>
(subscribes to title formatting updates for the playing or displayed track to be received as script-messages)

)ABC",
      "input.conf help");
}

BOOL CMpvInputPreferences::OnInitDialog(CWindow, LPARAM) {
  UINT header_size = pref_page_header_font_size;
  UINT text_size = 11;

  HDC hdc = GetDC();
  if (hdc != NULL) {
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    text_size = (text_size * dpi) / 72;
    header_size = (header_size * dpi) / 72;
  }

  edit_font =
      CreateFont(text_size, 0, 0, 0, FW_REGULAR, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, FIXED_PITCH, NULL);
  CEdit edit = ((CEdit)GetDlgItem(IDC_EDIT2));
  edit.SetFont(edit_font);

  sep_font =
      CreateFont(header_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                 CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));
  ((CStatic)GetDlgItem(IDC_STATIC_SECTION11)).SetFont(sep_font);

  pfc::string_formatter path;
  filesystem::g_get_native_path(core_api::get_profile_path(), path);
  path.add_filename("mpv");
  path.add_filename("input.conf");
  file::ptr config;
  pfc::string8 contents;
  try {
    filesystem::g_open(config, path, filesystem::open_mode_read, fb2k::noAbort);
    config->read_string_raw(contents, fb2k::noAbort);
  } catch (...) {
    contents = "";
  }

  edit.SetLimitText(0);
  uSetWindowText(edit, contents.c_str());

  dirty = false;

  return FALSE;
}

void CMpvInputPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

t_uint32 CMpvInputPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvInputPreferences::reset() {
  dirty = true;
  OnChanged();
}

void CMpvInputPreferences::apply() {
  pfc::string content = uGetWindowText(GetDlgItem(IDC_EDIT2));

  pfc::string_formatter path;
  filesystem::g_get_native_path(core_api::get_profile_path(), path);
  path.add_filename("mpv");
  try {
    filesystem::g_create_directory(path, fb2k::noAbort);
  } catch (exception_io_already_exists) {
  } catch (...) {
    popup_message::g_complain("Error creating mpv configuration directory");
    return;
  }

  path.add_filename("input.conf");

  file::ptr config;
  try {
    filesystem::g_open(config, path, filesystem::open_mode_write_new,
                       fb2k::noAbort);
    config->write_string_raw(content.c_str(), fb2k::noAbort);
  } catch (...) {
    popup_message::g_complain("Error writing mpv.conf");
    return;
  }

  mpv_player::restart();

  dirty = false;
  OnChanged();
}

bool CMpvInputPreferences::HasChanged() { return dirty; }

void CMpvInputPreferences::OnChanged() { m_callback->on_state_changed(); }

static const GUID guid_mpv_branch = {
    0xe73c725e,
    0xd85b,
    0x47f2,
    {0x87, 0x35, 0xb4, 0xce, 0x14, 0x17, 0xa5, 0x41}};
static preferences_branch_factory mpv_branch(guid_mpv_branch,
                                             preferences_page::guid_tools,
                                             "mpv");
static const GUID guid_mpv_advanced = {
    0x5d238b4b,
    0xb556,
    0x4a48,
    {0x89, 0xf9, 0xa6, 0x52, 0x87, 0xde, 0xc2, 0x59}};
static preferences_branch_factory mpv_advanced_branch(guid_mpv_advanced,
                                                      guid_mpv_branch,
                                                      "Advanced", 99);

class preferences_page_mpv_player_impl
    : public preferences_page_impl<CMpvPlayerPreferences> {
 public:
  double get_sort_priority() override { return 1; }
  const char* get_name() override { return "Player"; }
  GUID get_guid() override {
    static const GUID guid = {0x11c90957,
                              0xf691,
                              0x4c23,
                              {0xb5, 0x87, 0x8, 0x9e, 0x5d, 0xfa, 0x14, 0x7a}};

    return guid;
  }
  GUID get_parent_guid() override { return guid_mpv_branch; }
};

static preferences_page_factory_t<preferences_page_mpv_player_impl>
    g_preferences_page_mpv_player_impl_factory;

class preferences_page_mpv_thumbnails_impl
    : public preferences_page_impl<CMpvThumbnailPreferences> {
 public:
  double get_sort_priority() override { return 3; }
  const char* get_name() override { return "Thumbnails"; }
  GUID get_guid() override {
    static const GUID guid = {0xa99227a2,
                              0x7fe8,
                              0x4ed0,
                              {0x85, 0xdf, 0xe2, 0xef, 0x91, 0x94, 0x5, 0xa1}};

    return guid;
  }
  GUID get_parent_guid() override { return guid_mpv_branch; }
};

static preferences_page_factory_t<preferences_page_mpv_thumbnails_impl>
    g_preferences_page_mpv_thumbnails_impl_factory;

class preferences_page_mpv_osc_impl
    : public preferences_page_impl<CMpvOscPreferences> {
 public:
  double get_sort_priority() override { return 2; }
  const char* get_name() override { return "On-screen control"; }
  GUID get_guid() override {
    static const GUID guid = {0x4607e664,
                              0x1aa9,
                              0x48b1,
                              {0xa7, 0x64, 0x2e, 0xaa, 0xab, 0xe1, 0xfb, 0x86}};

    return guid;
  }
  GUID get_parent_guid() override { return guid_mpv_branch; }
};

static preferences_page_factory_t<preferences_page_mpv_osc_impl>
    g_preferences_page_mpv_osc_impl_factory;

class preferences_page_mpv_conf_impl
    : public preferences_page_impl<CMpvConfPreferences> {
 public:
  double get_sort_priority() override { return 10; }
  const char* get_name() override { return "mpv.conf"; }
  GUID get_guid() override {
    static const GUID guid = {0xbfd12c90,
                              0x129a,
                              0x4350,
                              {0x8b, 0x22, 0xb7, 0x5e, 0x30, 0x1e, 0x75, 0x6b}};

    return guid;
  }
  GUID get_parent_guid() override { return guid_mpv_advanced; }
};

static preferences_page_factory_t<preferences_page_mpv_conf_impl>
    g_preferences_page_mpv_conf_impl_factory;

class preferences_page_mpv_input_impl
    : public preferences_page_impl<CMpvInputPreferences> {
 public:
  double get_sort_priority() override { return 11; }
  const char* get_name() override { return "input.conf"; }
  GUID get_guid() override {
    static const GUID guid = {0x802b847d,
                              0xd0cb,
                              0x46a9,
                              {0x85, 0xd, 0xb7, 0x0, 0x65, 0x2f, 0x9, 0xd4}};

    return guid;
  }
  GUID get_parent_guid() override { return guid_mpv_advanced; }
};

static preferences_page_factory_t<preferences_page_mpv_input_impl>
    g_preferences_page_mpv_input_impl_factory;
}  // namespace mpv