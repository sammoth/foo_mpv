#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"
#include "mpv_container.h"
#include "preferences.h"
#include "resource.h"

namespace mpv {
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
static const GUID guid_cfg_thumb_cache = {
    0xd194fb34,
    0x6a16,
    0x4683,
    {0xbe, 0x67, 0x41, 0x13, 0x5a, 0x35, 0x2, 0x2f}};
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

cfg_bool cfg_video_enabled(guid_cfg_video_enabled, true);

cfg_uint cfg_bg_color(guid_cfg_bg_color, 0);
cfg_bool cfg_black_fullscreen(guid_cfg_black_fullscreen, true);
cfg_bool cfg_stop_hidden(guid_cfg_stop_hidden, true);
cfg_uint cfg_panel_metric(guid_cfg_panel_metric, 0);

static const char* cfg_popup_titleformat_default =
    "%title% - %artist%[' ('%album%')']";
static cfg_string cfg_popup_titleformat(guid_cfg_popup_titleformat,
                                        cfg_popup_titleformat_default);

cfg_bool cfg_thumbs(guid_cfg_thumbnails, true);
cfg_uint cfg_thumb_cover_type(guid_cfg_thum_cover_type, 0);
cfg_bool cfg_thumb_group_longest(guid_cfg_item_in_group, false);
cfg_bool cfg_thumb_group_override(guid_cfg_group_override, true);
cfg_uint cfg_thumb_size(guid_cfg_thumb_size, 1);
cfg_uint cfg_thumb_seek(guid_cfg_thumb_seek, 30);
cfg_bool cfg_thumb_histogram(guid_cfg_thumb_histogram, false);
cfg_bool cfg_thumb_cache(guid_cfg_thumb_cache, true);
cfg_uint cfg_thumb_cache_size(guid_cfg_thumb_cache_size, 0);
cfg_uint cfg_thumb_cache_format(guid_cfg_cache_format, 0);
cfg_bool cfg_thumb_filter(guid_cfg_filter, false);
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

advconfig_checkbox_factory cfg_logging("Enable verbose console logging",
                                       guid_cfg_logging, guid_cfg_branch, 0,
                                       false);
advconfig_checkbox_factory cfg_mpv_logfile("Enable mpv log file",
                                           guid_cfg_native_logging,
                                           guid_cfg_branch, 0, false);

static titleformat_object::ptr popup_titlefomat_script;
static search_filter::ptr thumb_filter;

void get_popup_title(pfc::string8& s) {
  if (popup_titlefomat_script.is_empty()) {
    static_api_ptr_t<titleformat_compiler>()->compile_safe(
        popup_titlefomat_script, cfg_popup_titleformat);
  }

  playback_control::get()->playback_format_title(
      NULL, s, popup_titlefomat_script, NULL,
      playback_control::t_display_level::display_level_all);
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

class CMpvPreferences : public CDialogImpl<CMpvPreferences>,
                        public preferences_page_instance {
 public:
  CMpvPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback), button_brush(CreateSolidBrush(cfg_bg_color)) {}

  enum { IDD = IDD_MPV_PREFS };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMyPreferences)
  MSG_WM_INITDIALOG(OnInitDialog);
  MSG_WM_CTLCOLORBTN(on_color_button);
  MSG_WM_HSCROLL(OnScroll);
  COMMAND_HANDLER_EX(IDC_BUTTON_BG, BN_CLICKED, OnBgClick);
  COMMAND_HANDLER_EX(IDC_CHECK_FSBG, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_STOP, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_EDIT_POPUP, EN_CHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_EDIT_PATTERN, EN_CHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_THUMBNAILS, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_THUMB_CACHE, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_RADIO_FIRSTINGROUP, BN_CLICKED, OnEditChange)
  COMMAND_HANDLER_EX(IDC_RADIO_LONGESTINGROUP, BN_CLICKED, OnEditChange)
  COMMAND_HANDLER_EX(IDC_CHECK_HISTOGRAM, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_COVERTYPE, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_GROUPOVERRIDE, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_CHECK_FILTER, BN_CLICKED, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_FORMAT, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_CACHESIZE, CBN_SELCHANGE, OnEditChange);
  COMMAND_HANDLER_EX(IDC_COMBO_THUMBSIZE, CBN_SELCHANGE, OnEditChange);
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
};

HBRUSH CMpvPreferences::on_color_button(HDC wp, HWND lp) {
  if (lp == GetDlgItem(IDC_BUTTON_BG)) {
    return button_brush;
  }
  return NULL;
}

BOOL CMpvPreferences::OnInitDialog(CWindow, LPARAM) {
  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_popup_titleformat);
  uSetDlgItemText(m_hWnd, IDC_EDIT_PATTERN, cfg_thumb_pattern);

  bg_col = cfg_bg_color.get_value();
  button_brush = CreateSolidBrush(bg_col);

  CheckDlgButton(IDC_CHECK_FSBG, cfg_black_fullscreen);
  CheckDlgButton(IDC_CHECK_STOP, cfg_stop_hidden);
  CheckDlgButton(IDC_CHECK_THUMBNAILS, cfg_thumbs);
  CheckDlgButton(IDC_CHECK_HISTOGRAM, cfg_thumb_histogram);
  CheckDlgButton(IDC_CHECK_THUMB_CACHE, cfg_thumb_cache);
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

  CComboBox combo_panelmetric = (CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC);
  combo_panelmetric.AddString(L"Area");
  combo_panelmetric.AddString(L"Width");
  combo_panelmetric.AddString(L"Height");
  combo_panelmetric.SetCurSel(cfg_panel_metric);

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
  slider_seek.SetRangeMax(80);
  slider_seek.SetPos(cfg_thumb_seek);

  set_controls_enabled();

  dirty = false;

  return FALSE;
}

void CMpvPreferences::OnBgClick(UINT, int, CWindow) {
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
    OnChanged();
  }
}

void CMpvPreferences::OnEditChange(UINT, int, CWindow) {
  dirty = true;
  OnChanged();
}

void CMpvPreferences::OnScroll(UINT, int, CWindow) {
  dirty = true;
  m_callback->on_state_changed();
}

t_uint32 CMpvPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvPreferences::reset() {
  bg_col = 0;
  button_brush = CreateSolidBrush(bg_col);

  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_popup_titleformat_default);
  uSetDlgItemText(m_hWnd, IDC_EDIT_PATTERN, cfg_thumb_pattern_default);

  CheckDlgButton(IDC_CHECK_FSBG, true);
  CheckDlgButton(IDC_CHECK_STOP, true);
  CheckDlgButton(IDC_CHECK_THUMBNAILS, true);
  CheckDlgButton(IDC_CHECK_FILTER, false);
  CheckDlgButton(IDC_CHECK_HISTOGRAM, false);
  CheckDlgButton(IDC_CHECK_THUMB_CACHE, true);
  CheckDlgButton(IDC_CHECK_GROUPOVERRIDE, true);
  CheckDlgButton(IDC_RADIO_LONGESTINGROUP, false);
  CheckDlgButton(IDC_RADIO_FIRSTINGROUP, true);

  ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).SetCurSel(0);
  ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).SetCurSel(0);

  ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_SEEK)).SetPos(30);

  OnChanged();
}

void CMpvPreferences::apply() {
  cfg_bg_color = bg_col;

  cfg_black_fullscreen = IsDlgButtonChecked(IDC_CHECK_FSBG);
  cfg_stop_hidden = IsDlgButtonChecked(IDC_CHECK_STOP);

  auto length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_POPUP));
  pfc::string format = uGetDlgItemText(m_hWnd, IDC_EDIT_POPUP);
  cfg_popup_titleformat.reset();
  cfg_popup_titleformat.set_string(format.get_ptr());

  static_api_ptr_t<titleformat_compiler>()->compile_safe(
      popup_titlefomat_script, cfg_popup_titleformat);

  length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_PATTERN));
  format = uGetDlgItemText(m_hWnd, IDC_EDIT_PATTERN);
  cfg_thumb_pattern.reset();
  cfg_thumb_pattern.set_string(format.get_ptr());

  try {
    thumb_filter =
        static_api_ptr_t<search_filter_manager>()->create(cfg_thumb_pattern);
  } catch (std::exception ex) {
    thumb_filter.reset();
  }

  cfg_thumbs = IsDlgButtonChecked(IDC_CHECK_THUMBNAILS);
  cfg_thumb_cache = IsDlgButtonChecked(IDC_CHECK_THUMB_CACHE);
  cfg_thumb_filter = IsDlgButtonChecked(IDC_CHECK_FILTER);
  cfg_thumb_histogram = IsDlgButtonChecked(IDC_CHECK_HISTOGRAM);
  cfg_thumb_group_longest = IsDlgButtonChecked(IDC_RADIO_LONGESTINGROUP);
  cfg_thumb_group_override = IsDlgButtonChecked(IDC_CHECK_GROUPOVERRIDE);

  cfg_thumb_cover_type =
      ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).GetCurSel();
  cfg_panel_metric =
      ((CComboBox)uGetDlgItem(IDC_COMBO_PANELMETRIC)).GetCurSel();
  cfg_thumb_size = ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).GetCurSel();
  cfg_thumb_cache_size =
      ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).GetCurSel();
  cfg_thumb_cache_format =
      ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).GetCurSel();

  cfg_thumb_seek = ((CTrackBarCtrl)uGetDlgItem(IDC_SLIDER_SEEK)).GetPos();

  mpv::invalidate_all_containers();
  dirty = false;
  OnChanged();
}

bool CMpvPreferences::HasChanged() { return dirty; }

void CMpvPreferences::set_controls_enabled() {
  bool thumbs = IsDlgButtonChecked(IDC_CHECK_THUMBNAILS);
  bool cache = IsDlgButtonChecked(IDC_CHECK_THUMB_CACHE);
  bool pattern = IsDlgButtonChecked(IDC_CHECK_FILTER);

  ((CComboBox)uGetDlgItem(IDC_EDIT_PATTERN)).EnableWindow(thumbs && pattern);
  ((CComboBox)uGetDlgItem(IDC_CHECK_FILTER)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_COVERTYPE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_THUMBSIZE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_CACHESIZE)).EnableWindow(thumbs && cache);
  ((CComboBox)uGetDlgItem(IDC_RADIO_LONGESTINGROUP)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_RADIO_FIRSTINGROUP)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_CHECK_HISTOGRAM)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_CHECK_GROUPOVERRIDE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_CHECK_THUMB_CACHE)).EnableWindow(thumbs);
  ((CComboBox)uGetDlgItem(IDC_COMBO_FORMAT)).EnableWindow(thumbs && cache);
  ((CComboBox)uGetDlgItem(IDC_SLIDER_SEEK)).EnableWindow(thumbs);
}

void CMpvPreferences::OnChanged() {
  m_callback->on_state_changed();
  set_controls_enabled();
  //Invalidate();
}

class preferences_page_mpv_impl
    : public preferences_page_impl<CMpvPreferences> {
 public:
  const char* get_name() { return "mpv"; }
  GUID get_guid() {
    static const GUID guid = {0x11c90957,
                              0xf691,
                              0x4c23,
                              {0xb5, 0x87, 0x8, 0x9e, 0x5d, 0xfa, 0x14, 0x7a}};

    return guid;
  }
  GUID get_parent_guid() { return guid_tools; }
};

static preferences_page_factory_t<preferences_page_mpv_impl>
    g_preferences_page_mpv_impl_factory;

bool video_enabled() { return cfg_video_enabled; }
void set_video_enabled(bool enabled) { cfg_video_enabled = enabled; }
bool stop_when_hidden() { return cfg_stop_hidden; }
bool logging_enabled() { return cfg_logging; }
bool mpv_log_enabled() { return cfg_mpv_logfile; }
bool black_fullscreen() { return cfg_black_fullscreen; }
unsigned background_color() { return cfg_bg_color; }
t_uint64 hard_sync_threshold() { return cfg_hard_sync_threshold; }
t_uint64 hard_sync_interval() { return cfg_hard_sync_interval; }
t_uint64 max_drift() { return cfg_max_drift; }
}  // namespace mpv