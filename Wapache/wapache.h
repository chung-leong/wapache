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
	const char *reg_save_path;
	const char *def_icon_path;
	BOOL Javascript;
	BOOL Java;
	BOOL ActiveX;
	BOOL external;
	BOOL Theme;
	const char *app_title;
};

struct wa_process_rec : public process_rec {
	apr_pool_t *plog;
	const char *bin_root;
	const char *conf_name;
	const char *conf_root;
	const char *conf_short_name;
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
	const char *Method;
	ap_regex_t *Pattern;
	apr_array_header_t *Args;	
};

struct wa_win_config {
	ap_regex_t *TargetPattern;

	const char *IconPath;

	const char *DropDownMenu;
	const char *DefaultMenu;
	const char *ImageMenu;
	const char *ControlMenu;
	const char *TableMenu;
	const char *TextSelectMenu;
	const char *AnchorMenu;

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
	const char *Name;
	const char *Charset;
};

struct wa_regular_menu_config : public wa_menu_config {
	apr_array_header_t *Items;
};

struct wa_script_menu_config : public wa_menu_config {
	const char *Method;
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
	const char *Opener;
	const char *Url;
	const char *Target;
};

struct wa_menu_item_config {
	int Type;
	const char *Label;
	UINT Id;
	ap_regex_t *Pattern;
};

struct wa_std_menu_item_config : public wa_menu_item_config {
	int CommandId;
};

struct wa_std_env_menu_item_config : public wa_std_menu_item_config {
	const char *EnvVarName;
	const char *EnvVarVal;
};

struct wa_url_menu_item_config : public wa_menu_item_config {
	const char *Url;
	const char *Target;
};

struct wa_url_env_menu_item_config : public wa_url_menu_item_config {
	const char *EnvVarName;
	const char *EnvVarVal;
};

struct wa_js_menu_item_config : public wa_menu_item_config {
	const char *Method;
	apr_array_header_t *Args;	
};

struct wa_js_env_menu_item_config : public wa_js_menu_item_config {
	const char *EnvVarName;
	const char *EnvVarVal;
};

struct wa_dom_menu_item_config : public wa_menu_item_config {
	const char *ElemId;
	const char *PropName;
	const char *PropVal;
};

struct wa_dom_env_menu_item_config : public wa_dom_menu_item_config {
	const char *EnvVarName;
	const char *EnvVarVal;
};

struct wa_sub_menu_config : public wa_menu_item_config {
	const char *Name;
};

struct wa_sticon_config {
	const char *IconPath;
	const char *ToolTip;
	const char *CharSet;
	const char *Domain;
	bool AutoExit;
	const char *LeftClickMenu;
	int LeftClickItemIndex;
	const char *RightClickMenu;
	int RightClickItemIndex;
	const char *DoubleClickMenu;
	int DoubleClickItemIndex;
};

struct wa_font_config {
	const char *FilePath;
};

void wa_hack_core(void);

#endif