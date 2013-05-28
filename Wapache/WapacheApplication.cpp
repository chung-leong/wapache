#include "stdafx.h"
#include "WapacheApplication.h"
#include "WapacheWindow.h"
#include "WapacheTrayIcon.h"
#include "WapacheProtocol.h"
#include "apr_env.h"

APLOG_USE_MODULE(wa_core);

WapacheApplication Application;

typedef HWND (WINAPI *GetTaskmanWindowProc)(void);
GetTaskmanWindowProc GetTaskmanWindow;

WapacheApplication::WapacheApplication(void) 
{
	RefCount = 0;
	ApacheReady = false;
	Process = NULL;
	ServerConf = NULL;
	ClientConf = NULL;
	SessionEnvTables = NULL;
	CookieTable = NULL;
	EnvPool = NULL;
	MultiLang = NULL;
	MonitorConfThread = NULL;
	MonitorDocThread = NULL;
	TerminationSignal = NULL;
	ThreadId = GetCurrentThreadId();
	VariantInit(&ScriptSessionObject);

	TerminationSignal = CreateEvent(NULL, TRUE, FALSE, NULL);
	ConfigChangeSignal = CreateEvent(NULL, FALSE, TRUE, NULL);

	ThreadSemaphore = CreateSemaphore(NULL, MAX_THREAD, MAX_THREAD, NULL);
	ZeroMemory(Threads, sizeof(Threads));

	// get GetTaskmanWindow() if running on Windows XP and above
	OSVERSIONINFO os_info;
	os_info.dwOSVersionInfoSize = sizeof(os_info);
	GetVersionEx(&os_info);
	if(os_info.dwMajorVersion >= 5 && os_info.dwMinorVersion >= 1) {
		HMODULE lib = GetModuleHandle("user32.dll");
		GetTaskmanWindow = (GetTaskmanWindowProc) GetProcAddress(lib, "GetTaskmanWindow");
	}
	OriginalTaskbarGroupButtonIconIndex = -1;
	OriginalTaskbarGroupButtonData = NULL;
}

WapacheApplication::~WapacheApplication(void)
{
	CloseHandle(ThreadSemaphore);
	CloseHandle(TerminationSignal);
	CloseHandle(ConfigChangeSignal);
	//VariantClear(&ScriptSessionObject);
}

ULONG WapacheApplication::AddRef(void)
{
	return ++RefCount;
}

ULONG WapacheApplication::Release(void)
{
	if(--RefCount == 0) {
		PostQuitMessage(0);
		return 0;
	}
	return RefCount;
}

int WapacheApplication::ProcessKeyboardEvent(MSG *msg)
{
	if(msg->message == WM_SYSKEYDOWN || msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYUP || msg->message == WM_KEYUP) {
		return WapacheWindow::ProcessKeyboardEvent(msg);
	}
	return 0;
}

int WapacheApplication::Run(void)
{
	ApacheReady = InitializeApache() && StartApache();
	if(ApacheReady) {
		OleInitialize(NULL);

		HRESULT hr;
		IInternetSession *session;
		hr = CoInternetGetSession(0, &session, 0);
		if(SUCCEEDED(hr)) {
			hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC_SERVER, IID_IMultiLanguage, reinterpret_cast<void **>(&MultiLang));

			ExposeSpecialFolders();
			RetrieveSavedEnvironments();
			RetrieveSavedCookies();
			hr = session->RegisterNameSpace(ProtocolFactory, CLSID_NULL, L"http", 0, NULL, 0);
			if(SUCCEEDED(hr)) {
				WapacheWindow::OnApplicationStart();
				WapacheTrayIcon::OnApplicationStart();

				MSG msg;
				if(RefCount > 0) {

					DWORD threadId;
					MonitorConfThread = CreateThread(NULL, 0, MonitorConfProc, this, 0, &threadId);
					MonitorDocThread = CreateThread(NULL, 0, MonitorDocProc, this, 0, &threadId);

					Sleep(0);
					SetTaskbarGroupButton();
					while(GetMessage(&msg, NULL, 0, 0)) {
						if(msg.message == WM_CONTINUE) {
							MsgProc proc = reinterpret_cast<MsgProc>(msg.wParam);
							void *data = reinterpret_cast<void *>(msg.lParam);
							proc(data);
						}
						else {
							if(!ProcessKeyboardEvent(&msg)) {
								TranslateMessage(&msg);
								DispatchMessage(&msg);
							}
						}
					}
					SetEvent(TerminationSignal);
					WaitForSingleObject(MonitorConfThread, INFINITE);
					WaitForSingleObject(MonitorDocThread, INFINITE);
					RestoreTaskbarGroupButton();
				}

				WapacheWindow::OnApplicationEnd();
				WapacheTrayIcon::OnApplicationEnd();

				session->UnregisterNameSpace(ProtocolFactory, L"http");
				session->Release();
			}
			if(MultiLang) {
				MultiLang->Release();
			}
		}

		OleUninitialize();
	}
	return 0;
}

void WapacheApplication::Switch(MsgProc proc, void  *data)
{
	WPARAM wParam = reinterpret_cast<WPARAM>(proc);
	WPARAM lParam = reinterpret_cast<LPARAM>(data);
	PostThreadMessage(ThreadId, WM_CONTINUE, wParam, lParam);
}

void wa_report_error(const ap_errorlog_info *info, const char *errstr)
{
	MessageBox(NULL, errstr, "Error", MB_OK);
}

bool WapacheApplication::InitializeApache(void)
{
	// Visual C++ specific way of getting argc and argv
	int argc = __argc;
	const char * const *argv = __argv;

	apr_pool_t *pglobal;
    apr_pool_t *pconf;
    apr_pool_t *plog; /* Pool of log streams, reset _after_ each read of conf */
    apr_pool_t *ptemp; /* Pool for temporary config stuff, reset often */
    apr_pool_t *pcommands; /* Pool for -D, -C and -c switches */
    apr_status_t rv;

	// initialize APR
	apr_app_initialize(&argc, &argv, NULL);

	// create the Process record
    rv = apr_pool_create(&pglobal, NULL);
	ap_pglobal = pglobal;

	if(rv != APR_SUCCESS) {
		// need to call the function directly, since the hook is not set up yet
		wa_report_error(NULL, "apr_pool_create() failed to create initial context");		
		return false;
	}

	// create various memory pools
	apr_pool_tag(pglobal, "Process");
    apr_pool_create(&pconf, pglobal);
	apr_pool_tag(pconf, "pconf");
    apr_pool_create(&pcommands, pglobal);
    apr_pool_tag(pcommands, "pcommands");
    apr_pool_create(&plog, pglobal);
    apr_pool_tag(plog, "plog");
    apr_pool_create(&ptemp, pconf);
    apr_pool_tag(ptemp, "ptemp");

	// create the Process record
    Process = (wa_process_rec *) apr_palloc(pglobal, sizeof(wa_process_rec));
	Process->pconf = pconf;
    Process->pool = pglobal;
    Process->argc = argc;
    Process->argv = argv;
	Process->plog = plog;
    Process->short_name = apr_filepath_name_get(argv[0]);
	Process->bin_root = wa_filepath_dir_get(pglobal, argv[0]);
	ap_server_argv0 = Process->short_name;
	if(argc > 1) {
		Process->conf_name = argv[1];
		Process->conf_root = wa_filepath_dir_get(pglobal, Process->conf_name);
		Process->conf_short_name = apr_filepath_name_get(Process->conf_name);
	}
	else {
		Process->conf_short_name = "default.wcf";
		apr_filepath_merge((char **) &Process->conf_root, Process->bin_root, "../conf",
                            APR_FILEPATH_TRUENAME, pglobal);
		apr_filepath_merge(const_cast<char **>(&Process->conf_name), Process->conf_root, Process->conf_short_name, 0, pglobal);
	}

    ap_server_pre_read_config  = apr_array_make(pcommands, 1, sizeof(char *));
    ap_server_post_read_config = apr_array_make(pcommands, 1, sizeof(char *));
    ap_server_config_defines   = apr_array_make(pcommands, 1, sizeof(char *));

	// hack off internal modules that we don't need and insert our own
	wa_hack_core();

	ap_setup_prelinked_modules(Process);

	// attach the error log hook
    ap_hook_error_log(wa_report_error,NULL,NULL,APR_HOOK_REALLY_LAST);

	// run rewrite args hooks
    ap_run_rewrite_args(Process);

	// create UI config record
	ClientConf = (wa_client_config *) apr_palloc(pconf, sizeof(wa_client_config));
	ClientConf->sec_win = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->sec_menu = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->sec_sticon = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->next_cmd_id = 100;
	ClientConf->initial_urls = apr_array_make(pconf, 5, sizeof(void *));
	ClientConf->fonts = apr_array_make(pconf, 5, sizeof(void *));
	ClientConf->reg_save_path = "Software\\Apache Group\\Wapache";
	ClientConf->Javascript = true;
	ClientConf->Java = true;
	ClientConf->ActiveX = true;
	ClientConf->external = false;
	ClientConf->app_title = NULL;
	ClientConf->Theme = false;

	// create a hash table for storing session environment variables
    apr_pool_create(&EnvPool, pglobal);
	SessionEnvTables = apr_table_make(EnvPool, sizeof(apr_table_t *));

	// create a hash table for storing cookies
	CookieTable = apr_table_make(pglobal, sizeof(const char *));

	// create a hash table for storing program environment vars
	ProgramEnvVars = apr_table_make(pglobal, sizeof(const char*));

	apr_filepath_merge((char **) &ap_server_root, Process->bin_root, "../", APR_FILEPATH_TRUENAME, pglobal);

	// switch to the default server root
	chdir(ap_server_root);

    ServerConf = (wa_server_rec *) ap_read_config(Process, ptemp, Process->conf_name, &ap_conftree);
	if(!ServerConf) {
		return false;
	}

	if(!ServerConf->server_hostname) {
		ServerConf->server_hostname = "localhost";
	}
    if (ap_run_pre_config(pconf, plog, ptemp) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR, 0,
                     NULL, "Pre-configuration failed");
        return false;
    }

    ap_process_config_tree(ServerConf, ap_conftree, Process->pconf, ptemp);
    ap_fixup_virtual_hosts(pconf, ServerConf);
    ap_fini_vhost_config(pconf, ServerConf);
    apr_hook_sort_all();

    apr_pool_clear(plog);

    if ( ap_run_open_logs(pconf, plog, ptemp, ServerConf) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR,
                     0, NULL, "Unable to open logs");
        return false;
    }

    if ( ap_run_post_config(pconf, plog, ptemp, ServerConf) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR, 0,
                     NULL, "Configuration Failed");
        return false;
    }

	return true;
}

bool WapacheApplication::StartApache(void)
{
	apr_pool_t *plog = Process->plog; /* Pool of log streams, reset _after_ each read of conf */
    apr_pool_t *ptemp; /* Pool for temporary config stuff, reset often */
	apr_pool_t *pconf = Process->pconf;
	apr_pool_t *pchild;
	module **mod;

	// unregister all the hooks, since some might not be present
	// under the new configuration
    apr_hook_deregister_all();
    apr_pool_clear(pconf);

    for (mod = ap_prelinked_modules; *mod != NULL; mod++) {
        ap_register_hooks(*mod, pconf);
    }

	// attach the error log hook
    ap_hook_error_log(wa_report_error,NULL,NULL,APR_HOOK_REALLY_LAST);

	// create UI config record
	ClientConf = (wa_client_config *) apr_palloc(pconf, sizeof(wa_client_config));
	ClientConf->sec_win = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->sec_menu = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->sec_sticon = apr_array_make(pconf, 10, sizeof(void *));
	ClientConf->initial_urls = apr_array_make(pconf, 5, sizeof(void *));
	ClientConf->fonts = apr_array_make(pconf, 5, sizeof(void *));
	ClientConf->Javascript = true;
	ClientConf->Java = true;
	ClientConf->ActiveX = true;
	ClientConf->external = false;
	ClientConf->next_cmd_id = 100;
	ClientConf->reg_save_path = "Software\\Wapache\\Wapache";
	ClientConf->app_title = NULL;
	ClientConf->Theme = false;

    /* This is a hack until we finish the code so that it only reads
        * the config file once and just operates on the tree already in
        * memory.  rbb
        */
    ap_conftree = NULL;
    apr_pool_create(&ptemp, pconf);
    apr_pool_tag(ptemp, "ptemp");
	apr_filepath_merge((char **) &ap_server_root, Process->bin_root, "../", APR_FILEPATH_TRUENAME, pconf);

    ServerConf = (wa_server_rec *) ap_read_config(Process, ptemp, Process->conf_name, &ap_conftree);
	if(!ServerConf->server_hostname) {
		ServerConf->server_hostname = "localhost";
	}
    if (ap_run_pre_config(pconf, plog, ptemp) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR,
                        0, NULL, "Pre-configuration failed");
        return false;
    }

	if(!wa_process_config_tree(ServerConf, ap_conftree, Process->pconf, ptemp)) {
		return false;
	}
    ap_fixup_virtual_hosts(pconf, ServerConf);
    ap_fini_vhost_config(pconf, ServerConf);
    apr_hook_sort_all();
    apr_pool_clear(plog);
    if (ap_run_open_logs(pconf, plog, ptemp, ServerConf) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR,
                        0, NULL, "Unable to open logs");
		return false;
    }

    if (ap_run_post_config(pconf, plog, ptemp, ServerConf) != OK) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP |APLOG_ERR,
                        0, NULL, "Configuration Failed");
		return false;
    }

    apr_pool_destroy(ptemp);
    apr_pool_lock(pconf, 1);

	InstallPrivateFonts();

	// run child_init hooks 
    apr_pool_create(&pchild, pconf);
    apr_pool_tag(pchild, "pchild");
    ap_run_child_init(pchild, ServerConf);

    ap_run_optional_fn_retrieve();
	return true;
}

bool WapacheApplication::ShutDownApache(void) 
{
	UninstallPrivateFonts();
    apr_pool_lock(pconf, 0);

	return true;
}

void WapacheApplication::NotifyConfigChange(void *data) {
	Application.ApplyNewSettings();
	WapacheWindow::OnApacheConfigChange();
	WapacheTrayIcon::OnApacheConfigChange();
}

void WapacheApplication::ApplyNewSettings(void) {
	RestartApache();
}

bool WapacheApplication::RestartApache(void) 
{
	LockApacheConfig(WRITE_LOCK);
	ApacheReady = false;
	ShutDownApache();
	ApacheReady = StartApache();
	UnlockApacheConfig(WRITE_LOCK);
	return ApacheReady;
}

void WapacheApplication::UninitializeApache(void)
{
	apr_pool_lock(Process->pconf, 0);
    apr_pool_destroy(Process->pool); /* and destroy all descendent pools */
    apr_terminate();
	Process = NULL;
	ApacheReady = false;
}

bool WapacheApplication::LockApacheConfig(int lockType) 
{
	if(lockType == READ_LOCK && !ApacheReady) {
		return false;
	}
	return ConfigLock.Lock(lockType);
}

bool WapacheApplication::UnlockApacheConfig(int lockType) 
{
	return ConfigLock.Unlock(lockType);
}

bool WapacheApplication::LockApacheEnv(int lockType)
{
	return EnvLock.Lock(lockType);
}

bool WapacheApplication::UnlockApacheEnv(int lockType)
{
	return EnvLock.Unlock(lockType);
}

bool WapacheApplication::LockCookieTable(int lockType)
{
	return CookieLock.Lock(lockType);
}

bool WapacheApplication::UnlockCookieTable(int lockType)
{
	return CookieLock.Unlock(lockType);
}

bool WapacheApplication::RetrieveSavedEnvironment(HKEY hKey, const char *domain) 
{
	apr_table_t *env = (apr_table_t *) apr_table_get(SessionEnvTables, domain);
	if(!env) {
		domain = apr_pstrdup(EnvPool, domain);
		env = apr_table_make(EnvPool, sizeof(const char *));
		apr_table_setn(SessionEnvTables, domain, (char *) env);
	}
	LONG rv = ERROR_SUCCESS;
	for(int i = 0; i < rv == ERROR_SUCCESS; i++) {
		char name[256];
		char *key, *value;
		DWORD nameLen = 256, valueLen = 0, type;

		rv = RegEnumValue(hKey, i, name, &nameLen, NULL, &type, NULL, &valueLen);
		if(rv == ERROR_SUCCESS) {
			value = (char *) apr_palloc(EnvPool, valueLen);
			nameLen = 256;
			rv = RegEnumValue(hKey, i, name, &nameLen, NULL, &type, 
									reinterpret_cast<LPBYTE>(value), &valueLen);
			if(rv == ERROR_SUCCESS && type == REG_SZ) {
				key = apr_pstrdup(EnvPool, name);
				apr_table_setn(env, key, value);
			}
		}
	}
	return (rv == ERROR_SUCCESS);
}

bool WapacheApplication::RetrieveSavedEnvironments(void) 
{
	LONG rv;
	HKEY hAppKey = NULL, hEnvKey = NULL, hDomainKey = NULL;
	rv = RegOpenKeyEx(HKEY_CURRENT_USER, ClientConf->reg_save_path, 0, KEY_READ, &hAppKey);
	if(rv == ERROR_SUCCESS) {
		rv = RegOpenKeyEx(hAppKey, "Environment", 0, KEY_READ, &hEnvKey);
		RegCloseKey(hAppKey);
	}
	if(rv == ERROR_SUCCESS) {
		RetrieveSavedEnvironment(hEnvKey, ServerConf->server_hostname);
		for(int i = 0; i < rv == ERROR_SUCCESS; i++) {
			char domain[256];
			DWORD len = 256;
			rv = RegEnumKeyEx(hEnvKey, i, domain, &len,  NULL, NULL, NULL, NULL);
			if(rv == ERROR_SUCCESS) {
				rv = RegOpenKeyEx(hEnvKey, domain, 0, KEY_READ, &hDomainKey);
				if(rv == ERROR_SUCCESS) {
					RetrieveSavedEnvironment(hDomainKey, domain);
					RegCloseKey(hDomainKey);
				}
			}
		}
		RegCloseKey(hEnvKey);
	}
	return (rv == ERROR_SUCCESS);
}

bool WapacheApplication::RetrieveSavedCookies(void)
{
	LONG rv;
	HKEY hAppKey = NULL, hCookieKey = NULL;
	rv = RegOpenKeyEx(HKEY_CURRENT_USER, ClientConf->reg_save_path, 0, KEY_READ, &hAppKey);
	if(rv == ERROR_SUCCESS) {
		rv = RegOpenKeyEx(hAppKey, "Cookie", 0, KEY_READ, &hCookieKey);
		RegCloseKey(hAppKey);
	}

	if(rv == ERROR_SUCCESS) {
		LONG rv = ERROR_SUCCESS;
		for(int i = 0; i < rv == ERROR_SUCCESS; i++) {
			char name[256];
			char *key, *value;
			DWORD nameLen = 256, valueLen = 0, type;

			rv = RegEnumValue(hCookieKey, i, name, &nameLen, NULL, &type, NULL, &valueLen);
			if(rv == ERROR_SUCCESS) {
				value = (char *) apr_palloc(Process->pool, valueLen);
				nameLen = 256;
				rv = RegEnumValue(hCookieKey, i, name, &nameLen, NULL, &type, 
										reinterpret_cast<LPBYTE>(value), &valueLen);
				if(rv == ERROR_SUCCESS && type == REG_SZ) {
					if(nameLen == 0) {
						key = apr_pstrdup(Process->pool, ServerConf->server_hostname);
					}
					else {
						key = apr_pstrdup(Process->pool, name);
					}
					apr_table_setn(CookieTable, key, value);
				}
			}
		}
		return (rv == ERROR_SUCCESS);
		RegCloseKey(hCookieKey);
	}
	return (rv == ERROR_SUCCESS);
}

const char *WapacheApplication::GetEnvironmentVar(const char *domain, const char *key, 
												  bool persistent)
{
	apr_table_t *env = (apr_table_t *) apr_table_get(SessionEnvTables, domain);
	if(env) {
		return apr_table_get(env, key);
	}
	return NULL;
}

void WapacheApplication::SetEnvironmentVar(const char *domain, const char *key, 
										   const char *value, bool persistent)
{
	apr_table_t *env = (apr_table_t *) apr_table_get(SessionEnvTables, domain);
	if(!env) {
		domain = apr_pstrdup(EnvPool, domain);
		env = apr_table_make(EnvPool, sizeof(const char *));
		apr_table_setn(SessionEnvTables, domain, (const char *) env);
	}
	const char *oldVal = apr_table_get(env, key);
	if(!oldVal || strcmp(oldVal, value) != 0) {
		apr_table_set(env, key, value);
		if(persistent) {
			LONG rv;
			HKEY hAppKey = NULL, hEnvKey = NULL, hDomainKey = NULL;
			rv = RegCreateKeyEx(HKEY_CURRENT_USER, ClientConf->reg_save_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hAppKey, NULL);
			if(rv == ERROR_SUCCESS) {
				rv = RegCreateKeyEx(hAppKey, "Environment", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hEnvKey, NULL);
				RegCloseKey(hAppKey);
			}
			if(rv == ERROR_SUCCESS) {
				if(stricmp(domain, ServerConf->server_hostname) == 0) {
					hDomainKey = hEnvKey;
				}
				else {
					rv = RegCreateKeyEx(hEnvKey, domain, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hDomainKey, NULL);
					RegCloseKey(hEnvKey);
				}
			}
			if(rv == ERROR_SUCCESS) {
				RegSetValueEx(hDomainKey, key, 0, REG_SZ, 
									reinterpret_cast<const BYTE *>(value), strlen(value) + 1);
				RegCloseKey(hDomainKey);
			}
		}
	}
}

#define LONG_STRING_LEN 2048

extern "C" {
	void do_expand(apr_pool_t *pool, apr_table_t *env, char *input, char *buffer, int nbuf);
}

const char *WapacheApplication::ExpandEnvironmentStr(apr_pool_t *pool, const char *s, 
													 const char *domain)
{
	if(!s) {
		return NULL;
	}
	apr_table_t *env = (apr_table_t *) apr_table_get(SessionEnvTables, domain);
	if(env) {
		apr_size_t bufSize = LONG_STRING_LEN;
		char *buffer = NULL;
		do {
			bufSize += LONG_STRING_LEN;
			buffer = (char *) apr_palloc(pool, bufSize);		
			do_expand(pool, env, (char *) s, buffer, bufSize);
		} while(strlen(buffer) == bufSize - 1);
		return buffer;
	}
	else {
		return s;
	}
}

const char *WapacheApplication::GetCookie(const char *domain)
{
	return apr_table_get(CookieTable, domain);
}

void WapacheApplication::SetCookie(const char *domain, const char *cookie)
{
	const char *oldCookie = apr_table_get(CookieTable, domain);
	if(!oldCookie || strcmp(oldCookie, cookie) != 0) {
		apr_table_set(CookieTable, domain, cookie);
	}

	LONG rv;
	HKEY hAppKey = NULL, hCookieKey = NULL;
	rv = RegCreateKeyEx(HKEY_CURRENT_USER, ClientConf->reg_save_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hAppKey, NULL);
	if(rv == ERROR_SUCCESS) {
		rv = RegCreateKeyEx(hAppKey, "Cookie", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hCookieKey, NULL);
		RegCloseKey(hAppKey);
	}
	if(rv == ERROR_SUCCESS) {
		const char *keyName;
		if(stricmp(domain, ServerConf->server_hostname) == 0) {
			keyName = NULL;
		}
		else {
			keyName = domain;
		}
		RegSetValueEx(hCookieKey, keyName, 0, REG_SZ, 
							reinterpret_cast<const BYTE *>(cookie), strlen(cookie) + 1);
		RegCloseKey(hCookieKey);
	}		
}

DWORD WapacheApplication::GetCharsetCode(BSTR charset)
{
	HRESULT hr = E_NOINTERFACE;
    MIMECSETINFO charsetInfo;

	if(MultiLang) {
		hr = MultiLang->GetCharsetInfo(charset, &charsetInfo);
		if(SUCCEEDED(hr)) {
			return charsetInfo.uiCodePage;
		}
	}

	return 0;
}

DWORD WapacheApplication::GetCharsetCode(const char *charset)
{

	LPOLESTR charsetW;
	charsetW = CStrToWideString(charset, -1);
	DWORD codePage = GetCharsetCode(charsetW);
	SysFreeString(charsetW);

	return codePage;
}

LPOLESTR WapacheApplication::ConvertString(DWORD code, const char *s, int len)
{
	if(!s) {
		return NULL;
	}
	HRESULT hr;
	LPOLESTR ws = NULL;
	if(len == -1) {
		len = strlen(s);
	}
	UINT wLen = 0;
	UINT converted = 0;
	DWORD mode = 0;

	do {
		wLen += (len - converted);
		SysFreeString(ws);

		ws = SysAllocStringLen(NULL, wLen);
		converted = len;
		hr = MultiLang->ConvertStringToUnicode(&mode, code, const_cast<char *>(s), &converted, 
												ws, &wLen);
	} while(SUCCEEDED(hr) && converted < (UINT) len);

	if(SUCCEEDED(hr)) {
		return ws;
	}
	else {
		SysFreeString(ws);
		return CStrToWideString(s, len);
	}
}

wa_win_config default_std_win_config = {
	NULL,				// regexp
	NULL,				// IconPath
	NULL,				// DropDownMenu;
	NULL,				// DefaultMenu;
	NULL,				// ImageMenu;
	NULL,				// ControlMenu;
	NULL,				// TableMenu;
	NULL,				// TextSelectMenu;
	NULL,				// AnchorMenu;
	ALIGN_TOP,			// VertAlign
	ALIGN_LEFT,			// HorizAlign
	STANDARD_WINDOW,	// Type
	{ 0, 0 },			// RoundedCornerX
	{ 0, 0 },			// RoundedCornerY
	false,				// Modal
	true,				// Resizeable
	true,				// MaximizeButton
	true,				// MinimizeButton
	false,				// HelpButton
	true,				// SystemMenu
	false,				// Maximized
	false,				// FullScreen
	true,				// InnerBorder;
	true,				// 3DBorder;
	true,				// ScrollBar;
	false,				// FlatScrollBar;
	false,				// AutoComplete;
	false,				// FrameLess
	false,				// MaintainAspectRatio
	false,				// Theme
	{ 0, 0 },			// Left
	{ 0, 0 },			// Top
	{ 0, 0 },			// Right
	{ 0, 0 },			// Bottom
	{ 0, 0 },			// Width
	{ 0, 0 },			// Height
	{ PIXEL, 600 },		// ClientWidth
	{ PIXEL, 400 }		// ClientHeight
};

wa_win_config default_tool_win_config = {
	NULL,				// regexp
	NULL,				// IconPath
	NULL,				// DropDownMenu;
	NULL,				// DefaultMenu;
	NULL,				// ImageMenu;
	NULL,				// ControlMenu;
	NULL,				// TableMenu;
	NULL,				// TextSelectMenu;
	NULL,				// AnchorMenu;
	ALIGN_TOP,			// VertAlign
	ALIGN_LEFT,		// HorizAlign
	TOOL_WINDOW,		// Type
	{ 0, 0 },			// RoundedCornerX
	{ 0, 0 },			// RoundedCornerY
	false,				// Modal
	false,				// Resizeable
	false,				// MaximizeButton
	false,				// MinimizeButton
	false,				// HelpButton
	true,				// SystemMenu
	false,				// Maximized
	false,				// FullScreen
	true,				// InnerBorder
	true,				// 3DBorder
	true,				// ScrollBar
	false,				// FlatScrollBar
	false,				// AutoComplete
	false,				// frameless
	false,				// MaintainAspectRatio
	false,				// Theme
	{ 0, 0 },			// Left
	{ 0, 0 },			// Top
	{ 0, 0 },			// Right
	{ 0, 0 },			// Bottom
	{ 0, 0 },			// Width
	{ 0, 0 },			// Height
	{ PIXEL, 200 },		// ClientWidth
	{ PIXEL, 300 }		// ClientHeight
};

wa_win_config default_dlg_box_config = {
	NULL,				// regexp
	NULL,				// IconPath
	NULL,				// DropDownMenu;
	NULL,				// DefaultMenu;
	NULL,				// ImageMenu;
	NULL,				// ControlMenu;
	NULL,				// TableMenu;
	NULL,				// TextSelectMenu;
	NULL,				// AnchorMenu;
	ALIGN_CENTER,		// VertAlign
	ALIGN_CENTER,		// HorizAlign
	DIALOG_BOX,			// Type
	{ 0, 0 },			// RoundedCornerX
	{ 0, 0 },			// RoundedCornerY
	false,				// Modal
	false,				// Resizeable
	false,				// MaximizeButton
	false,				// MinimizeButton
	false,				// HelpButton
	true,				// SystemMenu
	false,				// Maximized
	false,				// FullScreen
	true,				// InnerBorder
	true,				// 3DBorder
	true,				// ScrollBar
	false,				// FlatScrollBar
	false,				// AutoComplete
	false,				// Frameless
	false,				// MaintainAspectRatio
	false,				// Theme
	{ 0, 0 },			// Left
	{ 0, 0 },			// Top
	{ 0, 0 },			// Right
	{ 0, 0 },			// Bottom
	{ 0, 0 },			// Width
	{ 0, 0 },			// Height
	{ PIXEL, 300 },		// ClientWidth
	{ PIXEL, 200 }		// ClientHeight
};

wa_win_config *WapacheApplication::GetWindowSettings(const char *name) {
	wa_win_config **confs = (wa_win_config **) ClientConf->sec_win->elts;
	int num = ClientConf->sec_win->nelts;

	for(int i = 0; i < num; i++) {
		if(confs[i]->TargetPattern) {
			if(ap_regexec(confs[i]->TargetPattern, name, 0, NULL, 0) == 0) {
				return confs[i];
			}
		}
		else {
			if(strlen(name) == 0) {
				return confs[i];
			}
		}
	}

	return &default_std_win_config;
}

HMENU WapacheApplication::BuildMenu(const char *menuName, IDispatch *target, bool popup) 
{
	HMENU hMenu = NULL;
	wa_menu_config *mconf = NULL;
	wa_menu_config **confs = (wa_menu_config **) ClientConf->sec_menu->elts;
	int num = ClientConf->sec_menu->nelts;

	for(int i = 0; i < num; i++) {
		if(stricmp(confs[i]->Name, menuName) == 0) {
			mconf = confs[i];
			break;
		}
	}

	if(mconf) {
		if(mconf->Type == REGULAR_MENU) {
			wa_regular_menu_config *rmconf = (wa_regular_menu_config *) mconf;
			wa_menu_item_config **items = (wa_menu_item_config **) rmconf->Items->elts;
			int itemCount = rmconf->Items->nelts;
			hMenu = (popup) ? CreatePopupMenu() : CreateMenu();
			if(hMenu) {
				for(int i = 0; i < itemCount; i++) {
					MENUITEMINFO mii;
					ZeroMemory(&mii, sizeof(mii));
					mii.cbSize = sizeof(mii);
					if(items[i]->Type == SUB_MENU) {
						wa_sub_menu_config *sItem = (wa_sub_menu_config *) items[i];
						mii.hSubMenu = BuildPopupMenu(sItem->Name, target);
						mii.fMask = MIIM_SUBMENU | MIIM_FTYPE | MIIM_STRING;
						mii.fType = MFT_STRING;
						mii.dwTypeData = const_cast<LPSTR>(items[i]->Label);
					}
					else if(items[i]->Type == SEPARATOR) {
						mii.fMask = MIIM_FTYPE;
						mii.fType = MFT_SEPARATOR;
					}
					else {
						mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID;
						mii.fType = MFT_STRING;
						mii.wID = items[i]->Id;
						mii.dwTypeData = const_cast<LPSTR>(items[i]->Label);
					}
					InsertMenuItem(hMenu, i, TRUE, &mii);
				}
			}
		} else if(mconf->Type == SCRIPT_MENU) {
			wa_script_menu_config *smconf = (wa_script_menu_config *) mconf;
			WapacheExecContext context;
			IHTMLElement *el;
			IHTMLDocument2 *doc;
			IDispatch *docDisp;
			HRESULT hr;

			hr = target->QueryInterface(&el);
			if(hr == S_OK) {
				hr = el->get_document(&docDisp);
				el->Release();
			}
			if(hr == S_OK) {
				hr = docDisp->QueryInterface(&doc);
				docDisp->Release();
			}

			if(hr == S_OK) {
				// use the targetted element as the argument
				VARIANT arg;
				VariantInit(&arg);
				V_VT(&arg) = VT_DISPATCH;
				V_DISPATCH(&arg) = target;

				VARIANT *results;
				int resultCount;
				WapacheWindow::InitExecContext(&context, doc);
				context.Pattern = smconf->Pattern;
				WapacheWindow::ExecJSMethod(&context, smconf->Method, &arg, 1, &results, &resultCount);
				if(resultCount > 0) {
					UINT id = SCRIPT_MENU_ID_START;
					hMenu = (popup) ? CreatePopupMenu() : CreateMenu();
					for(int i = 0; i < resultCount; i++) {
						AddScriptMenuItems(hMenu, &results[i], &id);
						VariantClear(&results[i]);
						if(i != resultCount - 1) {
							// add a separator
							AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						}
					}
				}
				WapacheWindow::ClearExecContext(&context);
				doc->Release();
			}		
		}
	}
	return hMenu;
}

bool WapacheApplication::AddScriptMenuItems(HMENU hMenu, VARIANT *items, UINT *pId) {
	if(V_VT(items) == VT_DISPATCH) {
		HRESULT hr;
		IDispatch *disp = V_DISPATCH(items);
		IEnumVARIANT *enumVar = NULL;
		DISPPARAMS dispParams;
		dispParams.cArgs = 0;
		dispParams.cNamedArgs = 0;
		VARIANT ret;
		VariantInit(&ret);
		hr = disp->Invoke(DISPID_NEWENUM, IID_NULL, 0x0409, DISPATCH_PROPERTYGET, &dispParams, &ret, NULL, NULL);
		if(hr == S_OK) {
			IUnknown *enumUnk = V_UNKNOWN(&ret);
			hr = enumUnk->QueryInterface(&enumVar);
			VariantClear(&ret);
		} else {
			// need to do this for IE9
			hr = disp->QueryInterface(&enumVar);
		}
		if(enumVar) {
			int index = GetMenuItemCount(hMenu);
			VARIANT item;
			VariantInit(&item);
			// IEnumVARIANT::Next() might return E_FAIL even when it retrieves the item correctly
			while(hr = enumVar->Next(1, &item, NULL), V_VT(&item) != VT_EMPTY) {
				MENUITEMINFO mii;
				ZeroMemory(&mii, sizeof(mii));
				mii.cbSize = sizeof(mii);
				if(V_VT(&item) == VT_DISPATCH) {
					IDispatch *itemDisp = V_DISPATCH(&item);
					// get the label
					VARIANT label, menu, radio;
					VariantInit(&label);
					VariantInit(&menu);
					VariantInit(&radio);
					mii.fMask |= MIIM_FTYPE | MIIM_STRING;
					mii.fType = MFT_STRING;
					if(GetProperty(itemDisp, L"label", &label, VT_BSTR)) {
						mii.dwTypeData = WideStringToCStr(V_BSTR(&label), -1);
						VariantClear(&label);
					} else {
						mii.dwTypeData = WideStringToCStr(L"undefined", -1);
					}
					if(GetProperty(itemDisp, L"menu", &menu, VT_DISPATCH)) {
						mii.hSubMenu = CreatePopupMenu();
						mii.fMask |= MIIM_SUBMENU;
						AddScriptMenuItems(mii.hSubMenu, &menu, pId);
						VariantClear(&menu);
					} else {
						mii.fMask |= MIIM_ID | MIIM_DATA;
						mii.wID = (*pId)++;

						if(GetProperty(itemDisp, L"radio", &radio, VT_BOOL)) {
							mii.fType |= MFT_RADIOCHECK;
							VariantClear(&radio);
						}

						// save a reference to item
						mii.dwItemData = (ULONG_PTR) itemDisp;
						itemDisp->AddRef();
					}
				} else if(V_VT(&item) == VT_BSTR && wcscmp(V_BSTR(&item), L"-") == 0) {
					mii.fMask |= MIIM_FTYPE | MIIM_DATA;
					mii.fType = MFT_SEPARATOR;
				}
				if(mii.fMask) {
					InsertMenuItem(hMenu, index++, TRUE, &mii);
					delete[] mii.dwTypeData;
				}
				VariantClear(&item);
			}
			enumVar->Release();
		}
	}
	return TRUE;
}

bool FreeMenuItemReference(HMENU hMenu, int pos, int id, void *data) {
	if(id >= SCRIPT_MENU_ID_START) {
		MENUITEMINFO mii;
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_DATA;
		if(GetMenuItemInfo(hMenu, pos, TRUE, &mii)) {
			IDispatch *itemDisp = (IDispatch *) mii.dwItemData;
			if(itemDisp) {
				itemDisp->Release();
			}
		}
	}
	return TRUE;
}

void WapacheApplication::DestroyMenu(HMENU hMenu) {
	EnumMenuRecursive(hMenu, FreeMenuItemReference, NULL);
	::DestroyMenu(hMenu);
}

wa_menu_item_config *WapacheApplication::FindMenuItem(UINT id) {
	wa_regular_menu_config **confs = (wa_regular_menu_config **) ClientConf->sec_menu->elts;
	int num = ClientConf->sec_menu->nelts;

	for(int i = 0; i < num; i++) {
		wa_menu_item_config **items = (wa_menu_item_config **) confs[i]->Items->elts;
		int numItems = confs[i]->Items->nelts;
		for(int j = 0; j < numItems; j++) {
			if(items[j]->Id == id) {
				return items[j];
			}
		}
	}
	return NULL;
}

void WapacheApplication::Exit(void) 
{
	PostQuitMessage(0);
}

void WapacheApplication::ExposeSpecialFolder(const char *envName, int CSIDL)
{
	char path[MAX_PATH];
	if(SHGetSpecialFolderPath(NULL, path, CSIDL, FALSE)) {
		apr_table_add(ProgramEnvVars, envName, path);
	}
}

void WapacheApplication::ExposeSpecialFolders(void)
{
	ExposeSpecialFolder("SHELL_ADMINTOOLS", CSIDL_ADMINTOOLS);
	ExposeSpecialFolder("SHELL_COMMON_ADMINTOOLS", CSIDL_COMMON_ADMINTOOLS);
	ExposeSpecialFolder("SHELL_APPDATA", CSIDL_APPDATA);
	ExposeSpecialFolder("SHELL_COMMON_APPDATA", CSIDL_COMMON_APPDATA);
	ExposeSpecialFolder("SHELL_DESKTOPDIRECTORY", CSIDL_DESKTOPDIRECTORY);
	ExposeSpecialFolder("SHELL_COMMON_DESKTOPDIRECTORY", CSIDL_COMMON_DESKTOPDIRECTORY);
	ExposeSpecialFolder("SHELL_MYDOCUMENTS", CSIDL_PERSONAL);
	ExposeSpecialFolder("SHELL_COMMON_DOCUMENTS", CSIDL_COMMON_DOCUMENTS);
	ExposeSpecialFolder("SHELL_NETHOOD", CSIDL_NETHOOD);
	ExposeSpecialFolder("SHELL_PRINTHOOD", CSIDL_PRINTHOOD);
	ExposeSpecialFolder("SHELL_LOCAL_APPDATA", CSIDL_LOCAL_APPDATA);
	ExposeSpecialFolder("SHELL_FONTS", CSIDL_FONTS);
	ExposeSpecialFolder("SHELL_FAVORITES", CSIDL_FAVORITES);
	ExposeSpecialFolder("SHELL_COMMON_FAVORITES", CSIDL_COMMON_FAVORITES);
	ExposeSpecialFolder("SHELL_WINDOWS", CSIDL_WINDOWS);
	ExposeSpecialFolder("SHELL_TEMPLATES", CSIDL_TEMPLATES);
	ExposeSpecialFolder("SHELL_SYSTEM", CSIDL_SYSTEM);
	ExposeSpecialFolder("SHELL_STARTUP", CSIDL_STARTUP);
	ExposeSpecialFolder("SHELL_COMMON_STARTUP", CSIDL_COMMON_STARTUP);
	ExposeSpecialFolder("SHELL_STARTMENU", CSIDL_STARTMENU);
	ExposeSpecialFolder("SHELL_COMMON_STARTMENU", CSIDL_COMMON_STARTMENU);
	ExposeSpecialFolder("SHELL_PROGRAMS", CSIDL_PROGRAMS);
	ExposeSpecialFolder("SHELL_PROGRAM_FILES", CSIDL_PROGRAM_FILES);
	ExposeSpecialFolder("SHELL_COMMON_PROGRAMS", CSIDL_COMMON_PROGRAMS);
	ExposeSpecialFolder("SHELL_MYPICTURES", CSIDL_MYPICTURES);
	ExposeSpecialFolder("SHELL_COMMON_PICTURES", CSIDL_COMMON_PICTURES);
	ExposeSpecialFolder("SHELL_TEMPLATES", CSIDL_TEMPLATES);
	ExposeSpecialFolder("SHELL_COMMON_TEMPLATES", CSIDL_COMMON_TEMPLATES);
	ExposeSpecialFolder("SHELL_SENDTO", CSIDL_SENDTO);
	ExposeSpecialFolder("SHELL_RECENT", CSIDL_RECENT);
	ExposeSpecialFolder("SHELL_PROFILE", CSIDL_PROFILE);
	ExposeSpecialFolder("SHELL_DRIVES", CSIDL_DRIVES);
}

struct name_chain {
    name_chain *next;
    server_addr_rec *sar;       /* the record causing it to be in
                                 * this chain (needed for port comparisons) */
    server_rec *server;         /* the server to use on a match */
};

bool WapacheApplication::IsInternalAddress(LPOLESTR url)
{
	bool internal = false;
	if(wcsnicmp(url, L"javascript", 10) == 0) {
		internal = true;
	}
	else {
		apr_pool_t *ptrans;
		apr_pool_create(&ptrans, Process->pool);
		UrlComponents uCom;
		ZeroMemory(&uCom, sizeof(uCom));
		if(ParseUrl(ptrans, &uCom, url)) {
			if(uCom.Scheme && stricmp(uCom.Scheme, "http") == 0) {
				server_rec *server;
				if(FindServer(uCom.Host, &server)) {
					internal = true;
				}
			}
		}
		apr_pool_destroy(ptrans);
	}
	return internal;
}

bool WapacheApplication::EnumServers(ServerEnumFunc func, void *data) 
{
	if(LockApacheConfig(READ_LOCK)) {
		server_rec *s = NULL;
		bool cont = true;

		cont = func(ServerConf, data);

		if(cont) {
			// look for the virtual server record
			// this is harder than it should be because we don't have 
			// direct access to the virtual host table

			// create a temporary pool for the operation
			apr_pool_t *ptrans;
			apr_pool_create(&ptrans, Process->pool);

			// use a fake connection object to get the vhost info
			wa_conn_rec *c = (wa_conn_rec *) apr_pcalloc(ptrans, sizeof(wa_conn_rec));
			apr_sockaddr_t *addr = (apr_sockaddr_t *) apr_pcalloc(ptrans, sizeof(apr_sockaddr_t));

			addr->pool = ptrans;
			addr->family = APR_INET;
			addr->addr_str_len = sizeof(addr->sa);	
			addr->salen = addr->ipaddr_len = sizeof(addr->sa.sin.sin_addr);
			addr->sa.sin.sin_family = APR_INET;
			addr->sa.sin.sin_addr.s_addr = 0x0100007F;	// 127.0.0.1 (little endian)
			addr->ipaddr_ptr = &addr->sa.sin.sin_addr;

			c->local_addr = addr;
			c->client_addr = addr; 
			c->pool = ptrans;

			ap_update_vhost_given_ip(c);

			// we now have vhost_lookup_data

			name_chain *src;
			for (src = (name_chain *) c->vhost_lookup_data; cont && src; src = src->next) {
				cont = func(src->server, data);
			}
			apr_pool_destroy(ptrans);
		}
		UnlockApacheConfig(READ_LOCK);
		return true;
	}
	return false;
}

bool WapacheApplication::FindServer(const char *domain, server_rec **pServer) 
{
	server_rec *s = NULL;
	if(LockApacheConfig(READ_LOCK)) {

		if(ServerConf->server_hostname && !strcasecmp(domain, ServerConf->server_hostname)) {
			s = ServerConf;
		}

		if(!s) {
			// look for the virtual server record
			// this is harder than it should be because we don't have 
			// direct access to the virtual host table

			// create a temporary pool for the operation
			apr_pool_t *ptrans;
			apr_pool_create(&ptrans, Process->pool);

			// use a fake connection object to get the vhost info
			wa_conn_rec *c = (wa_conn_rec *) apr_pcalloc(ptrans, sizeof(wa_conn_rec));
			apr_sockaddr_t *addr = (apr_sockaddr_t *) apr_pcalloc(ptrans, sizeof(apr_sockaddr_t));

			addr->pool = ptrans;
			addr->family = APR_INET;
			addr->addr_str_len = sizeof(addr->sa);	
			addr->salen = addr->ipaddr_len = sizeof(addr->sa.sin.sin_addr);
			addr->sa.sin.sin_family = APR_INET;
			addr->sa.sin.sin_addr.s_addr = 0x0100007F;	// 127.0.0.1 (little endian)
			addr->ipaddr_ptr = &addr->sa.sin.sin_addr;

			c->local_addr = addr;
			c->client_addr = addr; 
			c->pool = ptrans;

			ap_update_vhost_given_ip(c);

			// we now have vhost_lookup_data

			name_chain *src;
			for (src = (name_chain *) c->vhost_lookup_data; src; src = src->next) {
				server_addr_rec *sar;

				sar = src->sar;

				/* does it match the virthost from the sar? */
				if (!strcasecmp(domain, sar->virthost)) {
					s = src->server;
				}
			}
			apr_pool_destroy(ptrans);
		}
		*pServer = s;
		UnlockApacheConfig(READ_LOCK);
	}
	return (s != NULL);
}

void WapacheApplication::GetEnvironmentVarType(const char *domain, const char *key, bool *isSession, bool *isPersistent)
{
	*isSession = false;
	*isPersistent = false;
	server_rec *server;
	if(FindServer(domain, &server)) {
	    void *sconf = server->module_config;
		wa_core_server_config *conf = 
				(wa_core_server_config *) ap_get_module_config(sconf, &core_module);

		int keyCount; 
		const char **keys;
		
		keyCount = conf->sav_env->nelts;
		keys = (const char **) conf->sav_env->elts;
		for(int i = 0; i < keyCount; i++) {
			if(stricmp(keys[i], key) == 0){
				*isPersistent = true;
				break;
			}
		}

		keyCount = conf->sess_env->nelts;
		keys = (const char **) conf->sess_env->elts;
		for(int i = 0; i < keyCount; i++) {
			if(stricmp(keys[i], key) == 0){
				*isPersistent = *isSession = true;
				break;
			}
		}
	}
}

bool WapacheApplication::CompareEnvironmentVar(const char *domain, const char *key, const char *value) {
	const char *current = GetEnvironmentVar(domain, key, false);
	if(current) {
		if(strcmp(current, value) == 0) {
			return true;
		}
	}
	return false;
}

DWORD WINAPI WapacheApplication::MonitorConfProc(LPVOID param) {
	WapacheApplication *self = reinterpret_cast<WapacheApplication *>(param);
	self->MonitorConfigurationFileChange();
	return 0;
}

FILETIME GetFileMTime(const char *path) {
	FILETIME mTime = { 0, 0 };
	HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if(file != INVALID_HANDLE_VALUE) {
		GetFileTime(file, NULL, NULL, &mTime);
		CloseHandle(file);
	}
	return mTime;
}

void WapacheApplication::MonitorConfigurationFileChange(void) {	
	HANDLE notification = FindFirstChangeNotification(Process->conf_root, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	HANDLE handles[2];
	handles[0] = TerminationSignal;
	handles[1] = notification;
	DWORD result;
	FILETIME mTime = GetFileMTime(Process->conf_name);
	while(result = WaitForMultipleObjects(2, handles, FALSE, INFINITE), result != WAIT_OBJECT_0) {
		FILETIME newMTime = GetFileMTime(Process->conf_name);
		if(CompareFileTime(&newMTime,  &mTime) != 0) {
			Switch(NotifyConfigChange, NULL);
		}
		mTime = newMTime;
		FindNextChangeNotification(notification);
	}
	FindCloseChangeNotification(notification);	
}

DWORD WINAPI WapacheApplication::MonitorDocProc(LPVOID param)
{
	WapacheApplication *self = reinterpret_cast<WapacheApplication *>(param);
	self->MonitorDocumentRootFileChange();
	return 0;
}

struct DocMonitorSiteData {
	const char *domain;
	const char **exts;
	const char *dir;
	int extCount;
	FILETIME mTime;
};

struct DocMonitorData {
	apr_pool_t *pool;
	int siteCount;
	int siteBufSize;
	DocMonitorSiteData *sites;
	HANDLE *waitObjs;
};

bool IsMonitoringFile(const char *fname, const char **exts, int extCount) {
	int len = strlen(fname);
	for(int i = 0; i < extCount; i++) {
		const char *ext = exts[i];
		int extLen = strlen(ext);
		if(len >= extLen) {
			const char *s = fname + (len - extLen);
			if(stricmp(s, ext) == 0) {
				return true;
			}
		}
	}
	return false;
}

FILETIME GetContentModifiedTime(const char *dir, const char **exts, int extCount)
{
	FILETIME mTime = { 0, 0 };
	char path[MAX_PATH];
	int len = min(strlen(dir), MAX_PATH - 2);
	if(len > 0) {
		memcpy(path, dir, len);
		if(path[len - 1] != '\\') {
			path[len++] = '\\';
		}
		path[len] = '*';
		path[len + 1] = '\0';

		WIN32_FIND_DATA wfd;
		HANDLE f = FindFirstFile(path, &wfd);
		if(f != INVALID_HANDLE_VALUE) {
			do {
				if(strcmp(wfd.cFileName, ".") && strcmp(wfd.cFileName, "..")) {
					if(wfd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
						if(len + strlen(wfd.cFileName) < MAX_PATH) {
							strcpy(path + len, wfd.cFileName);
							FILETIME dirMTime = GetContentModifiedTime(path, exts, extCount);
							if(CompareFileTime(&mTime, &dirMTime) < 0) {
								mTime = dirMTime;
							}
						}
					}
					else {					
						if(IsMonitoringFile(wfd.cFileName, exts, extCount)) {
							if(CompareFileTime(&mTime, &wfd.ftLastWriteTime) < 0) {
								mTime = wfd.ftLastWriteTime;
							}
						}
					}
				}
			} while(FindNextFile(f, &wfd));
			FindClose(f);
		}
	}
	return mTime;
}

bool AddServerDocRoot(server_rec *s, void *d) {
	wa_core_server_config *conf = (wa_core_server_config *) ap_get_module_config(s->module_config, &core_module);
	if(conf->monitor_ext && conf->monitor_ext->nelts > 0) {
		DocMonitorData *m = reinterpret_cast<DocMonitorData *>(d);

		if(m->siteCount >= m->siteBufSize) {
			m->siteBufSize += 4;
			DocMonitorSiteData *newBuf = reinterpret_cast<DocMonitorSiteData *>(apr_pcalloc(m->pool, sizeof(DocMonitorSiteData) * m->siteBufSize));
			CopyMemory(newBuf, m->sites, sizeof(DocMonitorSiteData) * m->siteCount);
			m->sites = newBuf;
		}
		DocMonitorSiteData *v = &m->sites[m->siteCount++];
		v->domain = apr_pstrdup(m->pool, s->server_hostname);
		char *dir = apr_pstrdup(m->pool, conf->ap_document_root);
		for(int i = 0; (size_t) i < strlen(dir); i++) {
			dir[i] = (dir[i] == '/') ? '\\' : dir[i];
		}
		v->dir = dir;

		const char **exts = (const char **) conf->monitor_ext->elts;
		int extCount = conf->monitor_ext->nelts;
		v->extCount = extCount;
		v->exts = reinterpret_cast<const char **>(apr_palloc(m->pool, sizeof(const char *) * extCount));
		for(int i = 0; i < extCount; i++) {
			v->exts[i] = apr_pstrdup(m->pool, exts[i]);
		}
		v->mTime = GetContentModifiedTime(v->dir, v->exts, v->extCount);
	}
	return true;
}

void WapacheApplication::MonitorDocumentRootFileChange(void)
{
	DocMonitorData m;
	ZeroMemory(&m, sizeof(m));
	apr_pool_create(&m.pool, Process->pool);
	m.waitObjs = reinterpret_cast<HANDLE *>(apr_pcalloc(m.pool, sizeof(HANDLE) * 2));
	m.waitObjs[0] = TerminationSignal;
	m.waitObjs[1] = ConfigChangeSignal;
	DWORD result;
	while(result = WaitForMultipleObjects(m.siteCount + 2, m.waitObjs, FALSE, INFINITE), result != WAIT_OBJECT_0) {
		if(result == WAIT_OBJECT_0 + 1) {
			for(int i = 0; i < m.siteCount; i++) {
				FindCloseChangeNotification(m.waitObjs[i + 2]);	
			}
			apr_pool_clear(m.pool);
			EnumServers(AddServerDocRoot, &m);
			
			m.waitObjs = reinterpret_cast<HANDLE *>(apr_pcalloc(m.pool, sizeof(HANDLE) * (m.siteCount + 2)));
			m.waitObjs[0] = TerminationSignal;
			m.waitObjs[1] = ConfigChangeSignal;			
			for(int i = 0; i < m.siteCount; i++) {
				m.waitObjs[i + 2] = FindFirstChangeNotification(m.sites[i].dir, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
			}
		}
		else {
			HANDLE notification = m.waitObjs[result - WAIT_OBJECT_0];
			DocMonitorSiteData *v = &m.sites[result - WAIT_OBJECT_0 - 2];
			FILETIME newMTime = GetContentModifiedTime(v->dir, v->exts, v->extCount);
			if(CompareFileTime(&newMTime, &v->mTime) != 0) {
				v->mTime = newMTime;
				Switch(NotifyDocumentChange, v);
			}
			FindNextChangeNotification(notification);
		}
	}
	apr_pool_destroy(m.pool);
}

void WapacheApplication::NotifyDocumentChange(void *data)
{
	DocMonitorSiteData *v = reinterpret_cast<DocMonitorSiteData *>(data);
	WapacheWindow::OnDocumentRootChange(v->domain);
}

DWORD WapacheApplication::RunThread(LPTHREAD_START_ROUTINE proc, LPVOID data) {
	if(WaitForSingleObject(ThreadSemaphore, MAX_THREAD_WAIT) == WAIT_TIMEOUT) {
		return false;
	}
	DWORD id = 0;
	for(int i = 0; i < MAX_THREAD && !id; i++) {
		if(!Threads[i].Data) {
			Threads[i].Data = data;
			Threads[i].Routine = proc;
			if(Threads[i].Handle) {
				id = Threads[i].Id;
				ResumeThread(Threads[i].Handle);
			}
			else {
				Threads[i].Handle = CreateThread(NULL, 0, ThreadProc, &Threads[i], 0, &id);
				Threads[i].Id = id;
			}
		}
	}
	return id;
}

DWORD WINAPI WapacheApplication::ThreadProc(LPVOID data)
{
	WapacheThread *thread = reinterpret_cast<WapacheThread *>(data);
	while(thread->Routine && thread->Data) {
		thread->Routine(thread->Data);
		ReleaseSemaphore(Application.ThreadSemaphore, 1, NULL);
		thread->Routine = NULL;
		thread->Data = NULL;
		SuspendThread(thread->Handle);
	}
	return 0;
}

bool WapacheApplication::SetTaskbarGroupButton(void) {
	bool success = false;
	if(GetTaskmanWindow) {
		const char *title = ClientConf->app_title;
		int title_len = (title) ? strlen(title) : -1;
		HWND taskman_win = GetTaskmanWindow();
		HWND toolbar = FindWindowEx(taskman_win, 0, "ToolbarWindow32", 0);
		DWORD taskman_pid = 0;
		HANDLE taskman;
		GetWindowThreadProcessId(taskman_win, &taskman_pid);
		taskman = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, taskman_pid);
		if(toolbar && taskman) {
			int button_count = SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0);

			if(button_count) {
				// allocate memory in Task Manager
				int taskman_buffer_size = max(sizeof(TBBUTTON) * button_count, sizeof(TBBUTTONINFO) + sizeof(char) * (title_len + 1));
				void *taskman_buffer = VirtualAllocEx(taskman, NULL, taskman_buffer_size, MEM_COMMIT, PAGE_READWRITE);
				DWORD read = 0, wrote = 0;

				TBBUTTON *buttons = new TBBUTTON[button_count];
				for(int i = 0; i < button_count; i++) {
					SendMessage(toolbar, TB_GETBUTTON, i, (LPARAM) &((TBBUTTON *) taskman_buffer)[i]);
				}

				// read the memory from Task Manager
				ReadProcessMemory(taskman, taskman_buffer, buttons, sizeof(TBBUTTON) * button_count, &read);

				// look for the group button--it's right in front of the window's button with hwnd == NULL
				for(int i = 1; i < button_count; i++) {
					HWND hwnd;
					ReadProcessMemory(taskman, (void *) buttons[i].dwData, &hwnd, sizeof(HWND), &read);
					if(hwnd && (hwnd == WapacheWindow::FirstWindow()->Handle())) {
						ReadProcessMemory(taskman, (void *) buttons[i - 1].dwData, &hwnd, sizeof(HWND), &read);
						if(!hwnd) {
							int group_index = i - 1;
							TBBUTTONINFO info;
							info.cbSize = sizeof(TBBUTTONINFO);
							info.dwMask = TBIF_BYINDEX | TBIF_IMAGE;

							// save the image index so the correct icon is removed by Task Manager
							OriginalTaskbarGroupButtonData = buttons[group_index].dwData;
							OriginalTaskbarGroupButtonIconIndex = buttons[group_index].iBitmap;

							// use the first window's icon
							info.iImage = buttons[i].iBitmap;

							if(title_len > 0) {
								// point to memory in Task Manager, right behind the struct
								info.pszText = ((char *) taskman_buffer) + sizeof(TBBUTTONINFO);
								WriteProcessMemory(taskman, info.pszText, title, title_len + 1, &wrote);
								info.dwMask |= TBIF_TEXT;
							}

							WriteProcessMemory(taskman, taskman_buffer, &info, sizeof(TBBUTTONINFO), &wrote);
							SendMessage(toolbar, TB_SETBUTTONINFO, group_index, (LPARAM) taskman_buffer);
							success = true;
							break;
						}
					}
				}
				
				VirtualFreeEx(taskman, taskman_buffer, 0, MEM_RELEASE);
			}
		}
		CloseHandle(taskman);
	}
	return success;
}

bool WapacheApplication::RestoreTaskbarGroupButton(void) {
	bool success = false;
	if(GetTaskmanWindow) {
		HWND taskman_win = GetTaskmanWindow();
		HWND toolbar = FindWindowEx(taskman_win, 0, "ToolbarWindow32", 0);
		DWORD taskman_pid = 0;
		HANDLE taskman;
		GetWindowThreadProcessId(taskman_win, &taskman_pid);
		taskman = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, taskman_pid);
		if(toolbar && taskman) {
			int button_count = SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0);

			if(button_count) {
				// allocate memory in Task Manager
				int taskman_buffer_size = max(sizeof(TBBUTTON) * button_count, sizeof(TBBUTTONINFO));
				void *taskman_buffer = VirtualAllocEx(taskman, NULL, taskman_buffer_size, MEM_COMMIT, PAGE_READWRITE);
				DWORD read = 0, wrote = 0;

				TBBUTTON *buttons = new TBBUTTON[button_count];
				for(int i = 0; i < button_count; i++) {
					SendMessage(toolbar, TB_GETBUTTON, i, (LPARAM) &((TBBUTTON *) taskman_buffer)[i]);
				}

				// read the memory from Task Manager
				ReadProcessMemory(taskman, taskman_buffer, buttons, sizeof(TBBUTTON) * button_count, &read);

				// look for the group button again
				for(int i = 1; i < button_count; i++) {
					HWND hwnd;
					ReadProcessMemory(taskman, (void *) buttons[i].dwData, &hwnd, sizeof(HWND), &read);
					if(buttons[i].dwData == OriginalTaskbarGroupButtonData) {
						int group_index = i;
						TBBUTTONINFO info;
						info.cbSize = sizeof(TBBUTTONINFO);
						info.dwMask = TBIF_BYINDEX | TBIF_IMAGE;

						// restore the index
						info.iImage = OriginalTaskbarGroupButtonIconIndex;
						WriteProcessMemory(taskman, taskman_buffer, &info, sizeof(TBBUTTONINFO), &wrote);
						SendMessage(toolbar, TB_SETBUTTONINFO, group_index, (LPARAM) taskman_buffer);
						success = true;
						break;
					}
				}
				
				VirtualFreeEx(taskman, taskman_buffer, 0, MEM_RELEASE);
			}
		}
		CloseHandle(taskman);
	}
	return success;
}

void WapacheApplication::InstallPrivateFonts(void) {
	wa_font_config **fonts = (wa_font_config **) Application.ClientConf->fonts->elts;
	int count = Application.ClientConf->fonts->nelts;
	int installed = 0;
	for(int i = 0; i < count; i++) {
		const char *path = fonts[i]->FilePath;
		installed += AddFontResourceEx(fonts[i]->FilePath, FR_PRIVATE, 0);
	}
}

void WapacheApplication::UninstallPrivateFonts(void) {
	wa_font_config **fonts = (wa_font_config **) Application.ClientConf->fonts->elts;
	int count = Application.ClientConf->fonts->nelts;
	for(int i = 0; i < count; i++) {
		RemoveFontResourceEx(fonts[i]->FilePath, FR_PRIVATE, 0);
	}
}