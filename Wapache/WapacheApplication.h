#ifndef __WAPACHEAPPLICATION_H
#define __WAPACHEAPPLICATION_H
#include "WapacheLock.h"
#define MAX_THREAD			128
#define MAX_THREAD_WAIT		5000

#define SCRIPT_MENU_ID_START	0x0000FFFF

typedef void (*MsgProc)(void *);
typedef bool (*ServerEnumFunc)(server_rec *, void *);

struct WapacheThread {
	HANDLE Handle;
	DWORD Id;
	LPVOID Data;
	LPTHREAD_START_ROUTINE Routine;
};

class WapacheApplication {
public:
	WapacheApplication(void);
	~WapacheApplication(void);

	ULONG AddRef(void);
	ULONG Release(void);

	int Run(void);
	DWORD RunThread(LPTHREAD_START_ROUTINE proc, LPVOID data);
	void Switch(MsgProc proc, void *data);
	void Exit(void);

	bool RestartApache(void);
	bool LockApacheConfig(int lockType);
	bool UnlockApacheConfig(int lockType);

	bool FindServer(const WCHAR *domain, server_rec **pServer);
	bool EnumServers(ServerEnumFunc func, void *data);
	bool IsInternalAddress(LPOLESTR url);
	void GetEnvironmentVarType(const WCHAR *domain, const WCHAR *key, bool *isSession, bool *isPersistent);

	wa_win_config *GetWindowSettings(const WCHAR *name);
	HMENU BuildMenu(const WCHAR *menuName, IDispatch *target, bool popup);
	HMENU BuildPopupMenu(const WCHAR *menuName, IDispatch *target = NULL) { return BuildMenu(menuName, target, true); };
	HMENU BuildDropdownMenu(const WCHAR *menuName) { return BuildMenu(menuName, NULL, false); };
	wa_menu_item_config *FindMenuItem(UINT id);
	bool AddScriptMenuItems(HMENU hMenu, VARIANT *items, UINT *pId);
	void DestroyMenu(HMENU hMenu);

	bool LockApacheEnv(int lockType);
	bool UnlockApacheEnv(int lockType);
	const WCHAR *GetEnvironmentVar(const WCHAR *domain, const WCHAR *key, bool persistent);
	void SetEnvironmentVar(const WCHAR *domain, const WCHAR *key, const WCHAR *value, bool persistent);
	bool CompareEnvironmentVar(const WCHAR *domain, const WCHAR *key, const WCHAR *value);
	const WCHAR *ExpandEnvironmentStr(apr_pool_t *pool, const WCHAR *s, const WCHAR *domain);

	bool LockCookieTable(int lockType);
	bool UnlockCookieTable(int lockType);
	const WCHAR *GetCookie(const WCHAR *domain);
	void SetCookie(const WCHAR *domain, const WCHAR *cookie);

	DWORD GetCharsetCode(BSTR charset);
	DWORD GetCharsetCode(const WCHAR *charset);
	LPOLESTR ConvertString(DWORD code, const char *s, int len);

private:
	// pass messages to ActiveX control
	int ProcessKeyboardEvent(MSG *msg);
	// initialize Apache related stuff
	bool InitializeApache(void);
	// shut down Apache related stuff
	void UninitializeApache(void);
	// scan the configuration tree, etc
	bool StartApache(void);
	// release resources
	bool ShutDownApache(void);

	// set group button icon and label
	bool SetTaskbarGroupButton(void);
	bool RestoreTaskbarGroupButton(void);

	// add private fonts
	void InstallPrivateFonts(void);
	void UninstallPrivateFonts(void);
	 
	// get environment variables from registry
	bool RetrieveSavedEnvironment(HKEY hKey, const WCHAR *domain);
	bool RetrieveSavedEnvironments(void);
	// get cookies from registery
	bool RetrieveSavedCookies(void);

	// add folder locations to environment
	void ExposeSpecialFolder(const WCHAR *envName, int CSIDL);
	void ExposeSpecialFolders(void);

	// monitor the configuration file for changes
	static DWORD WINAPI MonitorConfProc(LPVOID param);
	void MonitorConfigurationFileChange(void);
	static void NotifyConfigChange(void *data);
	void ApplyNewSettings(void);

	// monitor document roots for changes
	static DWORD WINAPI MonitorDocProc(LPVOID param);
	void MonitorDocumentRootFileChange(void);
	static void NotifyDocumentChange(void *data);

	static DWORD WINAPI ThreadProc(LPVOID data);

private:
	ULONG RefCount;
	bool ApacheReady;
	DWORD ThreadId;
	WapacheLock ConfigLock;
	WapacheLock EnvLock;
	WapacheLock CookieLock;
	IMultiLanguage *MultiLang;
	HANDLE MonitorConfThread;
	HANDLE MonitorDocThread;
	HANDLE TerminationSignal;
	HANDLE ConfigChangeSignal;

	HANDLE ThreadSemaphore;
	WapacheThread Threads[MAX_THREAD];

	int OriginalTaskbarGroupButtonIconIndex;
	DWORD_PTR OriginalTaskbarGroupButtonData;
public:
	// Apache records
    wa_process_rec *Process;	
    wa_server_rec *ServerConf;
	wa_client_config *ClientConf;
	apr_table_t *SessionEnvTables;
	apr_table_t *CookieTable;
	apr_table_t *ProgramEnvVars;
	apr_pool_t *EnvPool;
	VARIANT ScriptSessionObject;
};

extern WapacheApplication Application;
extern wa_win_config default_std_win_config;
extern wa_win_config default_tool_win_config;
extern wa_win_config default_dlg_box_config;

#define WM_CONTINUE			WM_USER + 7

#endif