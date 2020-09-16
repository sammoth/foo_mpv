#include "stdafx.h"
// PCH ^

#include "../helpers/atl-misc.h"
#include "mpv_container.h"
#include "preferences.h"
#include "resource.h"

namespace mpv {
static const GUID g_guid_cfg_mpv_bg_color = {
    0xb62c3ef, 0x3c6e, 0x4620, {0xbf, 0xa2, 0x24, 0xa, 0x5e, 0xdd, 0xbc, 0x4b}};
static const GUID g_guid_cfg_mpv_popup_titleformat = {
    0x811a9299,
    0x833d,
    0x4d38,
    {0xb3, 0xee, 0x89, 0x19, 0x8d, 0xed, 0x20, 0x77}};
static const GUID g_guid_cfg_mpv_black_fullscreen = {
    0x2e633cfd,
    0xbb88,
    0x4e69,
    {0xa0, 0xe4, 0x7f, 0x23, 0x2a, 0xdc, 0x5a, 0xd6}};
static const GUID guid_cfg_mpv_stop_hidden = {
    0x9de7e631,
    0x64f8,
    0x4047,
    {0x88, 0x39, 0x8f, 0x4a, 0x50, 0xa0, 0xb7, 0x2f}};
static const GUID guid_cfg_mpv_branch = {
    0xa8d3b2ca,
    0xa9a,
    0x4efc,
    {0xa4, 0x33, 0x32, 0x4d, 0x76, 0xcc, 0x8a, 0x33}};
static const GUID guid_cfg_mpv_max_drift = {
    0x69ee4b45,
    0x9688,
    0x45e8,
    {0x89, 0x44, 0x8a, 0xb0, 0x92, 0xd6, 0xf3, 0xf8}};
static const GUID guid_cfg_mpv_hard_sync_interval = {
    0xa095f426,
    0x3df7,
    0x434e,
    {0x91, 0x13, 0x19, 0x1a, 0x1b, 0x5f, 0xc1, 0xe5}};
static const GUID guid_cfg_mpv_hard_sync = {
    0xeffb3f43,
    0x60a0,
    0x4a2f,
    {0xbd, 0x78, 0x27, 0x43, 0xa1, 0x6d, 0xd2, 0xbb}};
static const GUID guid_cfg_mpv_logging = {
    0x8b74d741,
    0x232a,
    0x46d5,
    {0xa7, 0xee, 0x4, 0x89, 0xb1, 0x47, 0x43, 0xf0}};
static const GUID guid_cfg_mpv_native_logging = {
    0x3411741c,
    0x239,
    0x441d,
    {0x8a, 0x8e, 0x99, 0x83, 0x2a, 0xda, 0xe7, 0xd0}};
static const GUID guid_cfg_mpv_video_enabled = {
    0xe3a285f2,
    0x6804,
    0x4291,
    {0xa6, 0x8d, 0xb6, 0xac, 0x41, 0x89, 0x8c, 0x1d}};

cfg_bool cfg_mpv_video_enabled(guid_cfg_mpv_video_enabled, true);

cfg_uint cfg_mpv_bg_color(g_guid_cfg_mpv_bg_color, 0);
cfg_bool cfg_mpv_black_fullscreen(g_guid_cfg_mpv_black_fullscreen, true);
cfg_bool cfg_mpv_stop_hidden(guid_cfg_mpv_stop_hidden, true);

static cfg_string cfg_mpv_popup_titleformat(
    g_guid_cfg_mpv_popup_titleformat, "%title% - %artist%[' ('%album%')']");

static advconfig_branch_factory g_mpv_branch(
    "mpv", guid_cfg_mpv_branch, advconfig_branch::guid_branch_playback, 0);

advconfig_integer_factory cfg_mpv_max_drift(
    "Permitted timing drift (ms)", guid_cfg_mpv_max_drift, guid_cfg_mpv_branch,
    0, 20, 0, 1000, 0);
advconfig_integer_factory cfg_mpv_hard_sync("Hard sync threshold (ms)",
                                                   guid_cfg_mpv_hard_sync,
                                                   guid_cfg_mpv_branch, 0, 2000,
                                                   0, 10000, 0);
advconfig_integer_factory cfg_mpv_hard_sync_interval(
    "Minimum time between hard syncs (seconds)",
    guid_cfg_mpv_hard_sync_interval, guid_cfg_mpv_branch, 0, 10, 0, 30, 0);

advconfig_checkbox_factory cfg_mpv_logging(
    "Enable verbose console logging", guid_cfg_mpv_logging, guid_cfg_mpv_branch,
    0, false);
advconfig_checkbox_factory cfg_mpv_native_logging(
    "Enable mpv log file", guid_cfg_mpv_native_logging, guid_cfg_mpv_branch, 0,
    false);

static titleformat_object::ptr popup_titlefomat_script;

void get_popup_title(pfc::string8& s) {
  if (popup_titlefomat_script.is_empty()) {
    static_api_ptr_t<titleformat_compiler>()->compile_safe(
        popup_titlefomat_script, cfg_mpv_popup_titleformat);
  }

  playback_control::get()->playback_format_title(
      NULL, s, popup_titlefomat_script, NULL,
      playback_control::t_display_level::display_level_all);
}

class CMpvPreferences : public CDialogImpl<CMpvPreferences>,
                        public preferences_page_instance {
 public:
  CMpvPreferences(preferences_page_callback::ptr callback)
      : m_callback(callback),
        button_brush(CreateSolidBrush(cfg_mpv_bg_color)) {}

  enum { IDD = IDD_MPV_PREFS };

  t_uint32 get_state();
  void apply();
  void reset();

  BEGIN_MSG_MAP_EX(CMyPreferences)
  MSG_WM_INITDIALOG(OnInitDialog)
  MSG_WM_CTLCOLORBTN(on_color_button)
  COMMAND_HANDLER_EX(IDC_BUTTON_BG, BN_CLICKED, OnBgClick);
  COMMAND_HANDLER_EX(IDC_CHECK_FSBG, EN_CHANGE, OnEditChange)
  COMMAND_HANDLER_EX(IDC_CHECK_STOP, EN_CHANGE, OnEditChange)
  COMMAND_HANDLER_EX(IDC_EDIT_POPUP, EN_CHANGE, OnEditTextChange)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow, LPARAM);
  void OnBgClick(UINT, int, CWindow);
  void OnEditChange(UINT, int, CWindow);
  void OnEditTextChange(UINT, int, CWindow);
  bool HasChanged();
  void OnChanged();
  CBrush button_brush;
  HBRUSH on_color_button(HDC wp, HWND lp);
  bool edit_dirty = false;

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
  bg_col = cfg_mpv_bg_color.get_value();
  button_brush = CreateSolidBrush(bg_col);

  CheckDlgButton(IDC_CHECK_FSBG, cfg_mpv_black_fullscreen);
  CheckDlgButton(IDC_CHECK_STOP, cfg_mpv_stop_hidden);

  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, cfg_mpv_popup_titleformat);

  edit_dirty = false;

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

void CMpvPreferences::OnEditChange(UINT, int, CWindow) { OnChanged(); }
void CMpvPreferences::OnEditTextChange(UINT, int, CWindow) {
  edit_dirty = true;
  OnChanged();
}

t_uint32 CMpvPreferences::get_state() {
  t_uint32 state = preferences_state::resettable;
  if (HasChanged()) state |= preferences_state::changed;
  return state;
}

void CMpvPreferences::reset() {
  bg_col = 0;
  button_brush = CreateSolidBrush(bg_col);
  uSetDlgItemText(m_hWnd, IDC_EDIT_POPUP, "%title% - %artist%[ (%album%)]");
  CheckDlgButton(IDC_CHECK_FSBG, true);
  CheckDlgButton(IDC_CHECK_STOP, true);
  OnChanged();
}

void CMpvPreferences::apply() {
  cfg_mpv_bg_color = bg_col;

  cfg_mpv_black_fullscreen = IsDlgButtonChecked(IDC_CHECK_FSBG);
  cfg_mpv_stop_hidden = IsDlgButtonChecked(IDC_CHECK_STOP);

  auto length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_POPUP));
  pfc::string format = uGetDlgItemText(m_hWnd, IDC_EDIT_POPUP);
  cfg_mpv_popup_titleformat.reset();
  cfg_mpv_popup_titleformat.set_string(format.get_ptr());

  static_api_ptr_t<titleformat_compiler>()->compile_safe(
      popup_titlefomat_script, cfg_mpv_popup_titleformat);

  mpv::invalidate_all_containers();
  edit_dirty = false;
  OnChanged();
}

bool CMpvPreferences::HasChanged() {
  auto length = ::GetWindowTextLength(GetDlgItem(IDC_EDIT_POPUP));
  WCHAR* buf = new WCHAR[length + 1];
  GetDlgItemTextW(IDC_EDIT_POPUP, buf, length + 1);
  pfc::string8 str;
  str << buf;
  return edit_dirty ||
         IsDlgButtonChecked(IDC_CHECK_FSBG) != cfg_mpv_black_fullscreen ||
         IsDlgButtonChecked(IDC_CHECK_STOP) != cfg_mpv_stop_hidden ||
         bg_col != cfg_mpv_bg_color;
}

void CMpvPreferences::OnChanged() {
  m_callback->on_state_changed();
  Invalidate();
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

bool video_enabled() { return cfg_mpv_video_enabled; }
void set_video_enabled(bool enabled) { cfg_mpv_video_enabled = enabled; }
bool stop_when_hidden() { return cfg_mpv_stop_hidden; }
bool logging_enabled() { return cfg_mpv_logging; }
bool mpv_log_enabled() { return cfg_mpv_native_logging; }
bool black_fullscreen() { return cfg_mpv_black_fullscreen; }
unsigned background_color() { return cfg_mpv_bg_color; }
t_uint64 hard_sync_threshold() { return cfg_mpv_hard_sync; }
t_uint64 hard_sync_interval() { return cfg_mpv_hard_sync_interval; }
t_uint64 max_drift() { return cfg_mpv_max_drift; }
}  // namespace mpv