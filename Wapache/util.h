#ifndef __UTIL_H
#define __UTIL_H

int WideStringFragmentLen(LPCWSTR start, LPCWSTR end);
void WideStringTrim(LPCWSTR *s, LONG *len);
char *WideStringToCStr(LPCWSTR lpWideCharStr, int cchWideChar);
char *WideStringToPoolCStr(apr_pool_t *p, LPCWSTR lpWideCharStr, int cchWideChar);
char *WideStringsToPoolCStr(apr_pool_t *p, LPCWSTR *ppWideCharStr, ULONG cEl, const char *delimiter);

WCHAR *CStrToPoolWideString(apr_pool_t *p, LPCWSTR lpWideCharStr, int cchWideChar);
WCHAR *CStrToPoolWideString(apr_pool_t *p, LPCWSTR *ppWideCharStr, ULONG cEl, const char *delimiter);

LPOLESTR CStrToWideString(const char *s, int len);

LPOLESTR BuildRequestHeaderString(request_rec *r);
LPOLESTR BuildResponseHeaderString(request_rec *r);

bool IsLonelyFileBrigade(apr_bucket_brigade *bb, const char **filename);

void free_global_mem(void *data);
const char *wa_filepath_dir_get(apr_pool_t *pool, const char *path);

struct UrlComponents {
	apr_pool_t *pool;

	WCHAR *Scheme;
	WCHAR *User;
	WCHAR *Password;
	WCHAR *Host;
	WCHAR Port;
	WCHAR *Path;
	WCHAR *Query;
	WCHAR *Fragment;

	WCHAR *Full;
	WCHAR *Relative;
};

bool ParseUrl(apr_pool_t *p, UrlComponents *url, const char *s);
bool ParseUrl(apr_pool_t *p, UrlComponents *url, const WCHAR *s);

int chdir(const char *dir);

bool GetProperty(IDispatch *disp, LPOLESTR name, VARIANT *pResult, VARTYPE type);

typedef bool (*EnumMenuRecursiveProc)(HMENU, int, int, void *);

bool EnumMenuRecursive(HMENU hMenu, EnumMenuRecursiveProc func, void *data);

void debug_print(void *this_ptr, const char *text);

#ifdef SHOW_DEBUG_MESSAGE
#define debug(v)	debug_print(this, v);
#else 
#define debug(v)
#endif

#endif
