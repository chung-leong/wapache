#ifndef __WAPACHE_H
#define __WAPACHE_H

struct wa_core_server_config : public core_server_config {
	apr_array_header_t *sav_env;
	apr_array_header_t *sess_env;
	apr_table_t *def_env;
	apr_array_header_t *monitor_ext;
};

struct wa_client_config {
	apr_array_header_t *sec_win;
	apr_array_header_t *sec_menu;
	apr_array_header_t *sec_sticon;
	apr_array_header_t *initial_urls;
	apr_array_header_t *fonts;
	short next_cmd_id;
	const WCHAR *reg_save_path;
	const WCHAR *def_icon_path;
	BOOL Javascript;
	BOOL Java;
	BOOL ActiveX;
	BOOL external;
	BOOL Theme;
	const WCHAR *app_title;
};

struct wa_process_rec : public process_rec {
	apr_pool_t *plog;
	const WCHAR *bin_root;
	const WCHAR *conf_name;
	const WCHAR *conf_root;
	const WCHAR *conf_short_name;
};

struct wa_server_rec : public server_rec {
};

struct wa_conn_rec : public conn_rec {
	LPVOID object;
};

enum {
	PIXEL = 1,
	SCALED_PIXEL,
	PERCENT,
	INCH
};

struct wa_win_dim {
	short unit;
	short value;
};

enum {
	STANDARD_WINDOW,
	TOOL_WINDOW,
	DIALOG_BOX
};

enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
};

enum {
	ALIGN_TOP,
	ALIGN_MIDDLE,
	ALIGN_BOTTOM
};

struct wa_onclose_config {
	const WCHAR *Method;
	ap_regex_t *Pattern;
	apr_array_header_t *Args;	
};

struct wa_win_config {
	ap_regex_t *TargetPattern;

	const WCHAR *IconPath;

	const WCHAR *DropDownMenu;
	const WCHAR *DefaultMenu;
	const WCHAR *ImageMenu;
	const WCHAR *ControlMenu;
	const WCHAR *TableMenu;
	const WCHAR *TextSelectMenu;
	const WCHAR *AnchorMenu;

	int VertAlign;
	int HorizAlign;

	int Type;
	wa_win_dim RoundedCornerX;
	wa_win_dim RoundedCornerY;
	BOOL Modal;
	BOOL Resizeable;
	BOOL MaximizeButton;
	BOOL MinimizeButton;
	BOOL HelpButton;
	BOOL SystemMenu;
	BOOL Maximized;
	BOOL FullScreen;
	BOOL InnerBorder;
	BOOL ThreeDBorder;
	BOOL ScrollBar;
	BOOL FlatScrollBar;
	BOOL AutoComplete;
	BOOL Frameless;
	BOOL MaintainAspectRatio;
	BOOL Theme;

	wa_win_dim Left;
	wa_win_dim Top;
	wa_win_dim Right;
	wa_win_dim Bottom;
	wa_win_dim Width;
	wa_win_dim Height;
	wa_win_dim ClientWidth;
	wa_win_dim ClientHeight;
	wa_win_dim MinimumClientWidth;
	wa_win_dim MinimumClientHeight;

	wa_onclose_config *OnClose;
};

enum {
	REGULAR_MENU = 0,
	SCRIPT_MENU,
};

struct wa_menu_config {
	int Type;
	const WCHAR *Name;
};

struct wa_regular_menu_config : public wa_menu_config {
	apr_array_header_t *Items;
};

struct wa_script_menu_config : public wa_menu_config {
	const WCHAR *Method;
	ap_regex_t *Pattern;
};

enum {
	STD_MENU_ITEM = 1,
	JS_MENU_ITEM,
	URL_MENU_ITEM,
	DOM_MENU_ITEM,
	ENV_MENU_ITEM,
	STD_ENV_MENU_ITEM,
	JS_ENV_MENU_ITEM,
	URL_ENV_MENU_ITEM,
	DOM_ENV_MENU_ITEM,
	SEPARATOR,
	SUB_MENU,
};

struct wa_initial_url_config {
	const WCHAR *Opener;
	const WCHAR *Url;
	const WCHAR *Target;
};

struct wa_menu_item_config {
	int Type;
	const WCHAR *Label;
	UINT Id;
	ap_regex_t *Pattern;
};

struct wa_std_menu_item_config : public wa_menu_item_config {
	int CommandId;
};

struct wa_std_env_menu_item_config : public wa_std_menu_item_config {
	const WCHAR *EnvVarName;
	const WCHAR *EnvVarVal;
};

struct wa_url_menu_item_config : public wa_menu_item_config {
	const WCHAR *Url;
	const WCHAR *Target;
};

struct wa_url_env_menu_item_config : public wa_url_menu_item_config {
	const WCHAR *EnvVarName;
	const WCHAR *EnvVarVal;
};

struct wa_js_menu_item_config : public wa_menu_item_config {
	const WCHAR *Method;
	apr_array_header_t *Args;	
};

struct wa_js_env_menu_item_config : public wa_js_menu_item_config {
	const WCHAR *EnvVarName;
	const WCHAR *EnvVarVal;
};

struct wa_dom_menu_item_config : public wa_menu_item_config {
	const WCHAR *ElemId;
	const WCHAR *PropName;
	const WCHAR *PropVal;
};

struct wa_dom_env_menu_item_config : public wa_dom_menu_item_config {
	const WCHAR *EnvVarName;
	const WCHAR *EnvVarVal;
};

struct wa_sub_menu_config : public wa_menu_item_config {
	const WCHAR *Name;
};

struct wa_sticon_config {
	const WCHAR *IconPath;
	const WCHAR *ToolTip;
	const WCHAR *CharSet;
	const WCHAR *Domain;
	bool AutoExit;
	const WCHAR *LeftClickMenu;
	int LeftClickItemIndex;
	const WCHAR *RightClickMenu;
	int RightClickItemIndex;
	const WCHAR *DoubleClickMenu;
	int DoubleClickItemIndex;
};

struct wa_font_config {
	const WCHAR *FilePath;
};

void wa_hack_core(void);

#endif