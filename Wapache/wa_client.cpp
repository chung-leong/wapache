#include "stdafx.h"
#include "WapacheApplication.h"
#include "WapacheProtocol.h"

#define OR_WIN			0x80000000
#define OR_MENU			0x40000000
#define OR_TRAY			0x20000000
#define OR_SCRIPT_MENU	0x10000000

const char *mandatory_root = "HKEY_CURRENT_USER/Software/";

static const char *set_reg_root(cmd_parms *cmd, void *mconfig, const char *path)
{
    const command_rec *thiscmd = cmd->cmd;

	// convert backslashes to forward slashes first
	char *s = const_cast<char *>(path);
	for(int i = 0; s[i] != '\0'; i++) {
		if(s[i] == '\\') {
			if(s[i + 1] == '\0') {
				s[i] = '\0';
			}
			else {
				s[i] = '/';
			}
		}
	}

	if(strnicmp(path, mandatory_root, strlen(mandatory_root)) != 0) {
		return apr_pstrcat(cmd->pool, thiscmd->name, " must begin with '", 
			mandatory_root, "'", NULL);
	}

	// convert forward slashes to back slashes
	char *p = apr_pstrdup(cmd->pool, path + strlen("HKEY_CURRENT_USER/"));
	for(int i = 0; p[i] != '\0'; i++) {
		if(p[i] == '/') {
			p[i] = '\\';
		}
	}
	Application.ClientConf->reg_save_path = p;

	return NULL;
}

static const char *add_initial_url(cmd_parms *cmd, void *mconfig, const char *opener,
								   const char *url, const char *target)
{
	wa_initial_url_config *conf;
	wa_initial_url_config **conf_slot;

	if(!url) {
		// only one argument supplied
		url = opener;
		opener = NULL;
	}

	conf = (wa_initial_url_config *) apr_palloc(cmd->pool, sizeof(wa_initial_url_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Opener = opener;
	conf->Url = url;
	conf->Target = target;

	conf_slot = (wa_initial_url_config **) apr_array_push(Application.ClientConf->initial_urls);
	*conf_slot = conf;

	return NULL;
}

static const char *add_font(cmd_parms *cmd, void *mconfig, const char *font_path)
{
	wa_font_config *conf;
	wa_font_config **conf_slot;

	char *fullpath;
	apr_status_t rv;

	rv = apr_filepath_merge(&fullpath, Application.Process->conf_root, font_path, 0, cmd->pool);
	if(rv) {
		return apr_pstrcat(cmd->temp_pool, "'", font_path, "' is not a valid filepath", NULL);
	}

	for(int i = 0; fullpath[i] != '\0'; i++) {
		if(fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}

	conf = (wa_font_config *) apr_palloc(cmd->pool, sizeof(wa_font_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->FilePath = fullpath;

	conf_slot = (wa_font_config **) apr_array_push(Application.ClientConf->fonts);
	*conf_slot = conf;

	return NULL;
}

static const char *winsection(cmd_parms *cmd, void *mconfig, const char *arg)
{
	const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    wa_win_config *conf;
	wa_win_config **conf_slot;
    ap_regex_t *r = NULL;
    const command_rec *thiscmd = cmd->cmd;

    errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (errmsg != NULL) {
        return errmsg;
    }

    if (endp == NULL) {
		return apr_pstrcat(cmd->pool, "<", thiscmd->name, 
						   " > directive missing closing '>'", NULL);
    }

    arg = apr_pstrndup(cmd->pool, arg, endp - arg);

    if (arg) {
		cmd->path = ap_getword_conf(cmd->pool, &arg);
    }
	else {
		cmd->path = "";
	}

	if(cmd->path[0] != '\0') {
		char *pattern = apr_pstrcat(cmd->temp_pool, "^", cmd->path, "$", NULL);
		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	cmd->override = OR_WIN;

	conf = (wa_win_config *) apr_pcalloc(cmd->pool, sizeof(wa_win_config));
	memcpy(conf, thiscmd->cmd_data, sizeof(wa_win_config));
	conf->TargetPattern = r;

	conf_slot = (wa_win_config **) apr_array_push(Application.ClientConf->sec_win);
	*conf_slot = conf;

    errmsg = wa_walk_client_config(cmd->directive->first_child, cmd, conf);
	if (errmsg != NULL) {
        return errmsg;
	}

	cmd->path = old_path;
    cmd->override = old_overrides;

	return NULL;
}

static const char *set_win_align(cmd_parms *cmd, void *struct_ptr, const char *align)
{
    int offset = (int)(long)cmd->info;
	int *palign = (int *)((char *)struct_ptr + offset);

	if(stricmp(align, "Center") == 0 || stricmp(align, "Middle") == 0) {
		*palign = ALIGN_CENTER;
	}
	else if(stricmp(align, "Left") == 0 || stricmp(align, "Top") == 0) {
		*palign = ALIGN_LEFT;
	}
	else if(stricmp(align, "Right") == 0 || stricmp(align, "Bottom") == 0) {
		*palign = ALIGN_BOTTOM;
	}
	else {
		return "Invalid value";
	}
	return NULL;
}

static const char * set_client_flag_slot(cmd_parms *cmd, void *dummy, int flag) 
{
    int offset = (int)(long)cmd->info;
	void *struct_ptr = Application.ClientConf;
	BOOL *slot = (BOOL *)((char *)struct_ptr + offset);

	*slot = flag;

	return NULL;
}

const char *parse_dim(wa_win_dim *pdim, const char *arg) 
{
	static ap_regex_t *dimension_regex = NULL;
	if(!dimension_regex) {
		dimension_regex = ap_pregcomp(Application.Process->pool, "(-?\\d+)\\s*((px)|(sx)|(in)|(%))?$", AP_REG_ICASE);
	}

	ap_regmatch_t matches[3];

	if(ap_regexec(dimension_regex, arg, 3, matches, 0) == 0) {
		char buffer[16];
		apr_cpystrn(buffer, arg + matches[1].rm_so, min(16, matches[1].rm_eo - matches[1].rm_so + 1));
		pdim->value = atoi(buffer);
		pdim->unit = PIXEL;
		if(matches[2].rm_so > 0) {
			if(arg[matches[2].rm_so] == '%') {
				pdim->unit = PERCENT;
			}
			if(arg[matches[2].rm_so] == 'i') {
				pdim->unit = INCH;
			}
		}
	    return NULL;
	}
	else {
	    return "Invalid dimension";
	}
}

static const char * wa_set_dimension_slot(cmd_parms *cmd, void *struct_ptr, const char *arg)
{
    int offset = (int)(long)cmd->info;
	wa_win_dim *pdim = (wa_win_dim *)((char *)struct_ptr + offset);

	return parse_dim(pdim, arg);
}

static const char *menusection(cmd_parms *cmd, void *mconfig, const char *arg)
{
	const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    wa_regular_menu_config *conf;
	wa_regular_menu_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;

    errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (errmsg != NULL) {
        return errmsg;
    }

    if (endp == NULL) {
		return "<Menu > directive missing closing '>'";
    }

    arg = apr_pstrndup(cmd->pool, arg, endp - arg);

    if (!arg) {
        return "<Menu > block must specify a name";
	}

	cmd->path = ap_getword_conf(cmd->pool, &arg);
	cmd->override = OR_MENU;

	conf = (wa_regular_menu_config *) apr_pcalloc(cmd->pool, sizeof(wa_regular_menu_config));
	conf->Type = REGULAR_MENU;
	conf->Items = apr_array_make(cmd->pool, 10, sizeof(void *));

	conf_slot = (wa_regular_menu_config **) apr_array_push(Application.ClientConf->sec_menu);
	*conf_slot = conf;

    errmsg = wa_walk_client_config(cmd->directive->first_child, cmd, conf);
	if (errmsg != NULL) {
        return errmsg;
	}

	conf->Name = cmd->path;
	cmd->path = old_path;
    cmd->override = old_overrides;

	return NULL;
}

static const char *add_std_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf,
                                     const char *label, const char *win_name, const char *item_id)
{
    wa_std_menu_item_config *conf;
	wa_std_menu_item_config **conf_slot;
	int idm;
	ap_regex_t *r = NULL;

 	idm = CONSTANT_VALUE(MSHTML, item_id);

	if(!idm) {
		idm = atoi(item_id);
	}

	if(!idm) {
		return apr_pstrcat(cmd->pool, "Unrecognized command identifier: ", item_id, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_std_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_std_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = STD_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->CommandId = idm;
	conf->Id = Application.ClientConf->next_cmd_id++;

	conf_slot = (wa_std_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_std_env_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_std_env_menu_item_config *conf;
	wa_std_env_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
	int idm;
	ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
    const char *win_name = ap_getword_conf(cmd->pool, &args);
    const char *env_key = ap_getword_conf(cmd->pool, &args);
	const char *env_val = ap_getword_conf(cmd->pool, &args);
	const char *item_id = ap_getword_conf(cmd->pool, &args);

	if (*env_key == '\0' || *env_val == '\0' || *label == '\0' || *item_id == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires five arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

 	idm = CONSTANT_VALUE(MSHTML, item_id);

	if(!idm) {
		return apr_pstrcat(cmd->pool, "Unrecognized command identifier: ", item_id, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_std_env_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_std_env_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = STD_ENV_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->CommandId = idm;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->EnvVarName = env_key;
	conf->EnvVarVal = env_val;

	conf_slot = (wa_std_env_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_url_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_url_menu_item_config *conf;
	wa_url_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
	ap_regex_t *r;

    const char *label = ap_getword_conf(cmd->pool, &args);
    const char *win_name = ap_getword_conf(cmd->pool, &args);
	const char *url = ap_getword_conf(cmd->pool, &args);
	const char *target = ap_getword_conf(cmd->pool, &args);

	if (*label == '\0' || *url == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least four arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_url_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_url_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = URL_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->Url = url;
	conf->Target = target;

	conf_slot = (wa_url_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_url_env_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_url_env_menu_item_config *conf;
	wa_url_env_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
	ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
    const char *win_name = ap_getword_conf(cmd->pool, &args);
    const char *env_key = ap_getword_conf(cmd->pool, &args);
	const char *env_val = ap_getword_conf(cmd->pool, &args);
	const char *url = ap_getword_conf(cmd->pool, &args);
	const char *target = ap_getword_conf(cmd->pool, &args);

	if (*env_key == '\0' || *env_val == '\0' || *label == '\0' || *url == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least six arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}
	if(*target == '\0' ) {
		target = NULL;
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_url_env_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_url_env_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = URL_ENV_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->Url = url;
	conf->Target = target;
	conf->EnvVarName = env_key;
	conf->EnvVarVal = env_val;

	conf_slot = (wa_url_env_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_sub_menu(cmd_parms *cmd, wa_regular_menu_config *mconf,
                                const char *label, const char *name)
{
    wa_sub_menu_config *conf;
	wa_sub_menu_config **conf_slot;

	conf = (wa_sub_menu_config *) apr_palloc(cmd->pool, sizeof(wa_sub_menu_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = SUB_MENU;
	conf->Label = label;
	conf->Pattern = NULL;
	conf->Id = 0;
	conf->Name = name;

	conf_slot = (wa_sub_menu_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *set_onclose(cmd_parms *cmd, wa_win_config *wconf, const char *args)
{
	wa_onclose_config *conf;
    const command_rec *thiscmd = cmd->cmd;
    ap_regex_t *r = NULL;

	const char *win_name = ap_getword_conf(cmd->pool, &args);
	const char *method = ap_getword_conf(cmd->pool, &args);

	if (*method == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least two arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_onclose_config *) apr_palloc(cmd->pool, sizeof(wa_onclose_config));
	if(!conf) {
		return "Out of memory";
	}

	conf->Pattern = r;
	conf->Method = method;
	conf->Args = NULL;

	const char *a;
	while (*(a = ap_getword_conf(cmd->pool, &args)) != '\0') {
		if(!conf->Args) {
			conf->Args = apr_array_make(cmd->pool, 5, sizeof(const char *));
		}
		const char **arg_slot = (const char **) apr_array_push(conf->Args);
		*arg_slot = a;
	}

	wconf->OnClose = conf;

	return NULL;
}

static const char *add_js_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_js_menu_item_config *conf;
	wa_js_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
    ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
	const char *win_name = ap_getword_conf(cmd->pool, &args);
	const char *method = ap_getword_conf(cmd->pool, &args);

	if (*label == '\0' || *method == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least three arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_js_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_js_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}

	conf->Type = JS_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->Method = method;
	conf->Args = NULL;

	const char *a;
	while (*(a = ap_getword_conf(cmd->pool, &args)) != '\0') {
		if(!conf->Args) {
			conf->Args = apr_array_make(cmd->pool, 5, sizeof(const char *));
		}
		const char **arg_slot = (const char **) apr_array_push(conf->Args);
		*arg_slot = a;
	}

	conf_slot = (wa_js_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_js_env_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_js_env_menu_item_config *conf;
	wa_js_env_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
    ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
	const char *win_name = ap_getword_conf(cmd->pool, &args);
    const char *env_key = ap_getword_conf(cmd->pool, &args);
	const char *env_val = ap_getword_conf(cmd->pool, &args);
	const char *method = ap_getword_conf(cmd->pool, &args);

	if (*env_key == '\0' || *env_val == '\0' || *label == '\0' || *method == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least five arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_js_env_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_js_env_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}

	conf->Type = JS_ENV_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->EnvVarName = env_key;
	conf->EnvVarVal = env_val;
	conf->Method = method;
	conf->Args = NULL;

	const char *a;
	while (*(a = ap_getword_conf(cmd->pool, &args)) != '\0') {
		if(!conf->Args) {
			conf->Args = apr_array_make(cmd->pool, 5, sizeof(const char *));
		}
		const char **arg_slot = (const char **) apr_array_push(conf->Args);
		*arg_slot = a;
	}

	conf_slot = (wa_js_env_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_dom_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_dom_menu_item_config *conf;
	wa_dom_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
    ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
	const char *win_name = ap_getword_conf(cmd->pool, &args);
    const char *elem_id = ap_getword_conf(cmd->pool, &args);
	const char *prop = ap_getword_conf(cmd->pool, &args);
	const char *prop_val = ap_getword_conf(cmd->pool, &args);

	if (*elem_id == '\0' || *prop == '\0' || *label == '\0' || *prop_val == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least five arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_dom_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_dom_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = DOM_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->ElemId = elem_id;
	conf->PropName = prop;
	conf->PropVal = prop_val;

	conf_slot = (wa_dom_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_dom_env_menu_item(cmd_parms *cmd, wa_regular_menu_config *mconf, const char *args)
{
    wa_dom_env_menu_item_config *conf;
	wa_dom_env_menu_item_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;
    ap_regex_t *r = NULL;

    const char *label = ap_getword_conf(cmd->pool, &args);
	const char *win_name = ap_getword_conf(cmd->pool, &args);
    const char *env_key = ap_getword_conf(cmd->pool, &args);
	const char *env_val = ap_getword_conf(cmd->pool, &args);
    const char *elem_id = ap_getword_conf(cmd->pool, &args);
	const char *prop = ap_getword_conf(cmd->pool, &args);
	const char *prop_val = ap_getword_conf(cmd->pool, &args);

	if (*env_key == '\0' || *elem_id == '\0' || *prop == '\0'
	 || *env_val == '\0' || *label == '\0' || *prop_val == '\0') {
        return apr_pstrcat(cmd->pool, thiscmd->name,
                            " requires at least seven arguments",
                            thiscmd->errmsg ? ", " : NULL, thiscmd->errmsg, NULL);
	}

	if(win_name[0] != '\0') {
		const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

		r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
		if (!r) {
			return "Regex could not be compiled";
		}
	}

	conf = (wa_dom_env_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_dom_env_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = DOM_MENU_ITEM;
	conf->Label = label;
	conf->Pattern = r;
	conf->Id = Application.ClientConf->next_cmd_id++;
	conf->ElemId = elem_id;
	conf->PropName = prop;
	conf->PropVal = prop_val;
	conf->EnvVarName = env_key;
	conf->EnvVarVal = env_val;

	conf_slot = (wa_dom_env_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *add_menu_separator(cmd_parms *cmd, wa_regular_menu_config *mconf)
{
    wa_menu_item_config *conf;
	wa_menu_item_config **conf_slot;

	conf = (wa_menu_item_config *) apr_palloc(cmd->pool, sizeof(wa_menu_item_config));
	if(!conf) {
		return "Out of memory";
	}
	conf->Type = SEPARATOR;
	conf->Label = NULL;
	conf->Pattern = NULL;
	conf->Id = 0;

	conf_slot = (wa_menu_item_config **) apr_array_push(mconf->Items);
	*conf_slot = conf;

	return NULL;
}

static const char *scriptmenusection(cmd_parms *cmd, void *mconfig, const char *arg)
{
	const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    wa_script_menu_config *conf;
	wa_script_menu_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;

    errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (errmsg != NULL) {
        return errmsg;
    }

    if (endp == NULL) {
		return "<ScriptMenu > directive missing closing '>'";
    }

    arg = apr_pstrndup(cmd->pool, arg, endp - arg);

    if (!arg) {
        return "<ScriptMenu > block must specify a name";
	}

	cmd->path = ap_getword_conf(cmd->pool, &arg);
	cmd->override = OR_SCRIPT_MENU;

	conf = (wa_script_menu_config *) apr_pcalloc(cmd->pool, sizeof(wa_script_menu_config));
	conf->Type = SCRIPT_MENU;

	conf_slot = (wa_script_menu_config **) apr_array_push(Application.ClientConf->sec_menu);
	*conf_slot = conf;

    errmsg = wa_walk_client_config(cmd->directive->first_child, cmd, conf);
	if (errmsg != NULL) {
        return errmsg;
	}

	conf->Name = cmd->path;
	cmd->path = old_path;
    cmd->override = old_overrides;

	return NULL;
}

static const char *set_script_menu_method(cmd_parms *cmd, wa_script_menu_config *mconf,
                                     const char *win_name, const char *method_name)
{
	ap_regex_t *r;
	const char *pattern = apr_pstrcat(cmd->temp_pool, "^", win_name, "$", NULL);

	r = ap_pregcomp(cmd->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
	if (!r) {
		return "Regex could not be compiled";
	}
	mconf->Method = method_name;
	mconf->Pattern = r;
	return NULL;
}

static const char *set_title(cmd_parms *cmd, void *conf, const char *title)
{
	Application.ClientConf->app_title = title;

	return NULL;
}

static const char *set_icon_path(cmd_parms *cmd, void *conf, const char *path)
{
	wa_win_config *wconf;
	wa_sticon_config *stconf;
	char *fullpath;
	apr_status_t rv;

	rv = apr_filepath_merge(&fullpath, Application.Process->conf_root, path, 0, cmd->pool);
	if(rv) {
		return apr_pstrcat(cmd->temp_pool, "'", path, "' is not a valid filepath", NULL);
	}

	for(int i = 0; fullpath[i] != '\0'; i++) {
		if(fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	if(cmd->override & OR_WIN) {
		wconf = reinterpret_cast<wa_win_config *>(conf);
		wconf->IconPath = fullpath;
	}
	else if(cmd->override & OR_TRAY) {
		stconf = reinterpret_cast<wa_sticon_config *>(conf);
		stconf->IconPath = fullpath;
	}
	else {
		Application.ClientConf->def_icon_path = fullpath;
	}

	return NULL;
}

static const char *sticonsection(cmd_parms *cmd, void *mconfig, const char *arg)
{
	const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    wa_sticon_config *conf;
	wa_sticon_config **conf_slot;
    const command_rec *thiscmd = cmd->cmd;

    errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (errmsg != NULL) {
        return errmsg;
    }

    if (endp == NULL) {
		return apr_pstrcat(cmd->pool, "<", thiscmd->name, 
						   " > directive missing closing '>'", NULL);
    }

    arg = apr_pstrndup(cmd->pool, arg, endp - arg);

    if (arg) {
		cmd->path = ap_getword_conf(cmd->pool, &arg);
    }
	else {
		cmd->path = "";
	}

	cmd->override = OR_TRAY;

	conf = (wa_sticon_config *) apr_pcalloc(cmd->pool, sizeof(wa_sticon_config));

	conf_slot = (wa_sticon_config **) apr_array_push(Application.ClientConf->sec_sticon);
	*conf_slot = conf;

    errmsg = wa_walk_client_config(cmd->directive->first_child, cmd, conf);
	if (errmsg != NULL) {
        return errmsg;
	}

	cmd->path = old_path;
    cmd->override = old_overrides;

	return NULL;
}

static const char *set_menu_index(cmd_parms *cmd, void *struct_ptr, const char *menu, const char *index) 
{
    int offset = (int)(long)cmd->info;
	const char **menu_slot = (const char **)((char *)struct_ptr + offset);
	int *index_slot = (int *)(menu_slot + 1);

	int num = 0;
	if(index) {
		num = atoi(index);

		if(!num) {
			return "The second argument must be a number greater than one";
		}
	}
	*menu_slot = menu;
	*index_slot = num;

	return NULL;
}

const char *set_corner_radii(cmd_parms *cmd, wa_win_config *wconf, const char *x, const char *y) 
{
	const char *err; 
	
	err = parse_dim(&wconf->RoundedCornerX, x);
	if(err) {
		return err;
	}
	if(y) {
		err = parse_dim(&wconf->RoundedCornerY, y);
	}
	else {
		wconf->RoundedCornerY = wconf->RoundedCornerX;
	}
	return NULL;
}

static const command_rec wa_client_cmds[] = {
AP_INIT_TAKE1("ApplicationTitle", (cmd_func) set_title, NULL, RSRC_CONF,
	"the name of the application, used for XP taskbar group button"),
AP_INIT_TAKE1("DefaultIconPath", (cmd_func) set_icon_path, NULL, RSRC_CONF,
	"a path to an .ICO file"),
AP_INIT_TAKE1("Font", (cmd_func) add_font, NULL, RSRC_CONF,
	"a TrueType font made available to the application"),
AP_INIT_TAKE1("RegistryRoot", (cmd_func) set_reg_root, NULL, RSRC_CONF,
    "the root of the application's registry tree"),
AP_INIT_FLAG("Scripting", (cmd_func) set_client_flag_slot, (void*)APR_OFFSETOF(wa_client_config, Javascript), RSRC_CONF,
	"an on/off flag, specifying whether client-side scripting is enabled"),
AP_INIT_FLAG("Java", (cmd_func) set_client_flag_slot, (void*)APR_OFFSETOF(wa_client_config, Java), RSRC_CONF,
	"an on/off flag, specifying whether Java applets are allowed"),
AP_INIT_FLAG("ActiveX", (cmd_func) set_client_flag_slot, (void*)APR_OFFSETOF(wa_client_config, ActiveX), RSRC_CONF,
	"an on/off flag, specifying whether ActiveX controls are allowed"),
AP_INIT_FLAG("ExternalObject", (cmd_func) set_client_flag_slot, (void*)APR_OFFSETOF(wa_client_config, external), RSRC_CONF,
	"an on/off flag, specifying whether the external object should be available for scripting"),
AP_INIT_FLAG("DefaultTheme", (cmd_func) set_client_flag_slot, (void*)APR_OFFSETOF(wa_client_config, Theme), RSRC_CONF,
	"an on/off flag, indicating whether all windows should use XP interface theme"),
AP_INIT_RAW_ARGS("<StandardWindow", (cmd_func) winsection, &default_std_win_config, RSRC_CONF,
  "container for directives affecting the specified window"),
AP_INIT_RAW_ARGS("<ToolWindow", (cmd_func) winsection, &default_tool_win_config, RSRC_CONF,
  "container for directives affecting the specified window"),
AP_INIT_RAW_ARGS("<DialogBox", (cmd_func) winsection, &default_dlg_box_config, RSRC_CONF,
  "container for directives affecting the specified window"),
AP_INIT_RAW_ARGS("OnCloseHandler", (cmd_func) set_onclose, NULL, OR_WIN,
  "a window name, a function name, and a list of arguments"),
AP_INIT_TAKE1("VerticalAlign", (cmd_func) set_win_align, (void*)APR_OFFSETOF(wa_win_config, VertAlign), OR_WIN,
  "vertical alignment of the window"),
AP_INIT_TAKE1("HorizontalAlign", (cmd_func) set_win_align, (void*)APR_OFFSETOF(wa_win_config, HorizAlign), OR_WIN,
  "horizontalAlign alignment of the window"),
AP_INIT_TAKE1("Left", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Left), OR_WIN,
  "position of the window from the left"),
AP_INIT_TAKE1("Top", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Top), OR_WIN,
  "position of the window from the top"),
AP_INIT_TAKE1("Right", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Right), OR_WIN,
  "position of the window from the right"),
AP_INIT_TAKE1("Bottom", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Bottom), OR_WIN,
  "position of the window from the bottom"),
AP_INIT_TAKE1("Width", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Width), OR_WIN,
  "width of the window"),
AP_INIT_TAKE1("Height", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, Height), OR_WIN,
  "height of the window"),
AP_INIT_TAKE1("ClientWidth", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, ClientWidth), OR_WIN,
  "width of the window's client area"),
AP_INIT_TAKE1("ClientHeight", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, ClientHeight), OR_WIN,
  "height of the window's client area"),
AP_INIT_TAKE1("MinimumClientWidth", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, MinimumClientWidth), OR_WIN,
  "minimum width of the window's client area"),
AP_INIT_TAKE1("MinimumClientHeight", (cmd_func) wa_set_dimension_slot, (void*)APR_OFFSETOF(wa_win_config, MinimumClientHeight), OR_WIN,
  "minimum height of the window's client area"),
AP_INIT_TAKE1("DropDownMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, DropDownMenu), OR_WIN,
  "the name of the main menu"),
AP_INIT_TAKE1("DefaultContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, DefaultMenu), OR_WIN,
  "the name of the default context menu"),
AP_INIT_TAKE1("ImageContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, ImageMenu), OR_WIN,
  "the name of the context menu for images"),
AP_INIT_TAKE1("ControlContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, ControlMenu), OR_WIN,
  "the name of the context menu for form input controls"),
AP_INIT_TAKE1("TableContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, TableMenu), OR_WIN,
  "the name of the context menu for tables"),
AP_INIT_TAKE1("TextSelectionContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, TextSelectMenu), OR_WIN,
  "the name of the context menu for text selection"),
AP_INIT_TAKE1("AnchorContextMenu", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_win_config, AnchorMenu), OR_WIN,
  "the name of the context menu for hyperlinks"),
AP_INIT_FLAG("Modal", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, Modal), OR_WIN,
	"an on/off flag, indicating whether the dialog box is modal"),
AP_INIT_FLAG("Maximized", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, Maximized), OR_WIN,
	"an on/off flag, indicating whether the window should be maximized"),
AP_INIT_FLAG("FullScreen", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, FullScreen), OR_WIN,
	"an on/off flag, indicating whether the window should be fill the entire screen"),
AP_INIT_FLAG("MaximizeButton", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, MaximizeButton), OR_WIN,
	"an on/off flag, indicating whether the window has a maximize button"),
AP_INIT_FLAG("MinimizeButton", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, MinimizeButton), OR_WIN,
	"an on/off flag, indicating whether the window has a minimize button"),
AP_INIT_FLAG("Resizeable", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, Resizeable), OR_WIN,
	"an on/off flag, indicating whether the window is resizeable"),
AP_INIT_FLAG("HelpButton", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, HelpButton), OR_WIN,
	"an on/off flag, indicating whether the window has a help button"),
AP_INIT_FLAG("SystemMenu", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, SystemMenu), OR_WIN,
	"an on/off flag, indicating whether the window has a system menu"),
AP_INIT_FLAG("InnerBorder", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, InnerBorder), OR_WIN,
	"an on/off flag, indicating whether the window has an inner border"),
AP_INIT_FLAG("3DBorder", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, ThreeDBorder), OR_WIN,
	"an on/off flag, indicating whether borders have 3D appearance"),
AP_INIT_FLAG("ScrollBar", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, ScrollBar), OR_WIN,
	"an on/off flag, indicating whether the window has scroll bars"),
AP_INIT_FLAG("FlatScrollBar", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, FlatScrollBar), OR_WIN,
	"an on/off flag, indicating whether scroll bars should be flat"),
AP_INIT_FLAG("AutoComplete", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, AutoComplete), OR_WIN,
	"an on/off flag, indicating whether AutoComplete should be active"),
AP_INIT_TAKE12("RoundedCorners", (cmd_func) set_corner_radii, NULL, OR_WIN,
	"the radius/radii of the rounded corners"),
AP_INIT_FLAG("Frameless", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, Frameless), OR_WIN,
	"an on/off flag, indicating whether the window should be frameless"),
AP_INIT_FLAG("MaintainAspectRatio", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, MaintainAspectRatio), OR_WIN,
	"an on/off flag, indicating whether the aspect ratio of the window should be fixed"),
AP_INIT_FLAG("Theme", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_win_config, Theme), OR_WIN,
	"an on/off flag, indicating whether the window uses XP interface theme"),
AP_INIT_TAKE1("IconPath", (cmd_func) set_icon_path, NULL, OR_WIN | OR_TRAY,
	"a path to an .ICO file"),
AP_INIT_TAKE123("InitialUrl", (cmd_func) add_initial_url, NULL, RSRC_CONF,
	"the opener, a URL, and optionally, the name of the window"),
AP_INIT_RAW_ARGS("<Menu", (cmd_func) menusection, NULL, RSRC_CONF,
  "container for directives defining a context menu"),
AP_INIT_TAKE3("BasicMenuItem", (cmd_func) add_std_menu_item, NULL, OR_MENU, 
	"a label, a window name, and a command identifier"),
AP_INIT_RAW_ARGS("BasicEnvMenuItem", (cmd_func) add_std_env_menu_item, NULL, OR_MENU, 
	"a label, a window name, the name of the environment variable to set, the value, and a command identifier"),
AP_INIT_RAW_ARGS("UrlMenuItem", (cmd_func) add_url_menu_item, NULL, OR_MENU, 
	"a label, a window name, a URL, and the name of the target frame (optional)"),
AP_INIT_RAW_ARGS("UrlEnvMenuItem", (cmd_func) add_url_env_menu_item, NULL, OR_MENU,
	"a label, a window name, the name of the environment variable to set, the value, a URL,"
	" and the name of the target frame (optional)"),
AP_INIT_NO_ARGS("MenuSeparator", (cmd_func) add_menu_separator, NULL, OR_MENU, 
	"no arguments"),
AP_INIT_RAW_ARGS("ScriptMenuItem", (cmd_func) add_js_menu_item, NULL, OR_MENU,
	"a label, a window name, a function name, and a list of arguments"),
AP_INIT_RAW_ARGS("ScriptEnvMenuItem", (cmd_func) add_js_env_menu_item, NULL, OR_MENU,
	"a label, a window name, the name of the environment variable to set, the value, "
	"a function name, and a list of arguments"),
AP_INIT_RAW_ARGS("DOMMenuItem", (cmd_func) add_dom_menu_item, NULL, OR_MENU,
	"a label, a window name, the id the element, the property to set, and the value"),
AP_INIT_RAW_ARGS("DOMEnvMenuItem", (cmd_func) add_dom_env_menu_item, NULL, OR_MENU,
	"a label, a window name, the name of the environment variable to set, the value, "
	"the id the element, the property name, and the value of the property"),
AP_INIT_TAKE2("SubMenu", (cmd_func) add_sub_menu, NULL, OR_MENU, 
	"the name of the sub menu"),
AP_INIT_RAW_ARGS("<ScriptMenu", (cmd_func) scriptmenusection, NULL, RSRC_CONF,
  "container for directives defining a script-driven context menu"),
AP_INIT_TAKE2("ScriptMenuMethod", (cmd_func) set_script_menu_method, NULL, OR_SCRIPT_MENU, 
	"a function name"),
AP_INIT_RAW_ARGS("<SystemTrayIcon", (cmd_func) sticonsection, NULL, RSRC_CONF,
  "container for directives for specifying a System Tray icon"),
AP_INIT_TAKE1("ToolTip", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_sticon_config, ToolTip), OR_TRAY,
	"a tool tip message"),
AP_INIT_TAKE1("DispatchDomain", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_sticon_config, Domain), OR_TRAY,
	"a domain used for dispatching commands"),
AP_INIT_TAKE1("DispatchCharset", (cmd_func) ap_set_string_slot, (void*)APR_OFFSETOF(wa_sticon_config, CharSet), OR_TRAY,
	"a charset used for dispatching commands"),
AP_INIT_TAKE12("LeftClickMenu", (cmd_func) set_menu_index, (void*)APR_OFFSETOF(wa_sticon_config, LeftClickMenu), OR_TRAY,
	"a tool tip message"),
AP_INIT_TAKE12("RightClickMenu", (cmd_func) set_menu_index, (void*)APR_OFFSETOF(wa_sticon_config, RightClickMenu), OR_TRAY,
	"a tool tip message"),
AP_INIT_TAKE12("DoubleClickMenu", (cmd_func) set_menu_index, (void*)APR_OFFSETOF(wa_sticon_config, DoubleClickMenu), OR_TRAY,
	"a tool tip message"),
AP_INIT_FLAG("AutoExit", (cmd_func) ap_set_flag_slot, (void*)APR_OFFSETOF(wa_sticon_config, AutoExit), OR_TRAY,
	"an on/off flag, indicating if the system tray icon should disappear when all windows are closed"),
    { NULL }  
};

extern "C" {

module wa_client_module = {
    STANDARD20_MODULE_STUFF,
    NULL,							/* create per-directory config structure */
    NULL,							/* merge per-directory config structures */
    NULL,							/* create per-server config structure */
    NULL,							/* merge per-server config structures */
    wa_client_cmds,					/* command apr_table_t */
    NULL							/* register hooks */
};

}
