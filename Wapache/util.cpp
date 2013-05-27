#include "stdafx.h"
#include "util.h"
#include "WapacheApplication.h"
#include "guicon.h"

const char *wa_filepath_dir_get(apr_pool_t *pool, const char *path) {
	const char *s = strrchr(path, '\\');
	if(!s) {
		s = strrchr(path, '/');
	}
	if(s) {
		char *dir = apr_pstrndup(pool, path, strlen(path) - strlen(s));
		for(char *p = dir; *p != '\0'; p++) {
			if(*p == '\\') *p = '/';
		}
		return dir;
	}
	return "";
}

char *WideStringToCStr(LPCWSTR lpWideCharStr, int cchWideChar)
{
	if(lpWideCharStr) {
		// WideCharToMultiByte accounts for terminating 0 if cchWideChar is -1
		// otherwise no t0 is added
		int term_len = (cchWideChar == -1) ? 0 : 1;
		int len = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, NULL, 0, NULL, NULL);
		char *cstr = new char[len + term_len];
		if(cstr) {
			WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, cstr, len, NULL, NULL);
			// WideCharToMultiByte doesn't add terminating 0
			if(term_len) {
				cstr[len] = '\0';
			}
		}
		return cstr;
	}
	else {
		return NULL;
	}
}

char *WideStringToPoolCStr(apr_pool_t *p, LPCWSTR lpWideCharStr, int cchWideChar)
{
	if(lpWideCharStr) {
		int term_len = (cchWideChar == -1) ? 0 : 1;
		int len = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, NULL, 0, NULL, NULL);
		char *cstr = reinterpret_cast<char *>(apr_palloc(p, len + term_len));
		if(cstr) {
			WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, cstr, len, NULL, NULL);
			if(term_len) {
				cstr[len] = '\0';
			}
		}
		return cstr;
	}
	else {
		return NULL;
	}
}

char *WideStringsToPoolCStr(apr_pool_t *p, LPCWSTR *ppWideCharStr, ULONG cEl, const char *delimiter)
{
	if(ppWideCharStr && cEl > 0) {
		int delimiter_len = strlen(delimiter);
		int len = 0;
		// length of the strings concatenated
		for(ULONG i = 0; i < cEl; i++) {
			len += WideCharToMultiByte(CP_ACP, 0, ppWideCharStr[i], -1, NULL, 0, NULL, NULL) - 1;
		}
		// length of the delimiter
		len += (delimiter_len * (cEl - 1));
		char *cstr = reinterpret_cast<char *>(apr_palloc(p, len + 1));
		char *p = cstr;
		if(cstr) {
			for(ULONG i = 0; i < cEl; i++) {
				int copied = WideCharToMultiByte(CP_ACP, 0, ppWideCharStr[i], -1, p, len + 1, NULL, NULL) - 1;
				p += copied;
				len -= copied;
				if(i != cEl - 1) {
					strcpy(p, delimiter);
					p += delimiter_len;
					len -= delimiter_len;
				}
				else {
					p[0] = '\0';
				}
			}
		}
		return cstr;
	}
	else {
		return NULL;
	}
}

int WideStringFragmentLen(LPCWSTR start, LPCWSTR end) {
	if(start && end) {
		return (reinterpret_cast<long>(end) - reinterpret_cast<long>(start)) / sizeof(wchar_t);
	}
	else {
		return 0;
	}
}

void WideStringTrim(LPCWSTR *s, LONG *len) {
	if(*s && *len > 0) {
		while((*s)[0] == ' ' || (*s)[0] == '\r' || (*s)[0] == '\n' || (*s)[0] == 't') {
			(*s)++;
			(*len)--;
		}
		while((*s)[*len - 1] == ' ' || (*s)[*len - 1] == '\r' || (*s)[*len - 1] == '\n' || (*s)[*len - 1] == '\t') {
			(*len)--;
		}
	}
}

void free_global_mem(void *data) {
	HGLOBAL handle = GlobalHandle(data);
	GlobalUnlock(handle);
}

LPOLESTR CStrToWideString(const char *s, int len) {
	int term_len = (len == -1) ? 0 : 1;
	LONG wlen = MultiByteToWideChar(CP_ACP, 0, s, len, NULL, 0);
	BSTR bstr = SysAllocStringLen(NULL, wlen - term_len);
	if(bstr) {
		MultiByteToWideChar(CP_ACP, 0, s, len, bstr, wlen);
		if(term_len) {
			bstr[wlen] = '\0';
		}
	}
	return bstr;
}

struct wcheaders {
	int buf_len;
	int len;
	LPOLESTR str;
};

// callback functions for apr_table_do
int __calc_header_len(void *rec, const char *key, const char *value) 
{
	wcheaders *h = reinterpret_cast<wcheaders *>(rec);
	h->buf_len += MultiByteToWideChar(CP_ACP, 0, key, -1, NULL, 0) + 1;
	h->buf_len += MultiByteToWideChar(CP_ACP, 0, value, -1, NULL, 0) + 1;
	return 0;
}

int __concat_header(void *rec, const char *key, const char *value) 
{
	wcheaders *h = reinterpret_cast<wcheaders *>(rec);
	h->len += MultiByteToWideChar(CP_ACP, 0, key, -1, h->str + h->len, h->buf_len - h->len) - 1;
	h->len += MultiByteToWideChar(CP_ACP, 0, ": ", -1, h->str + h->len, h->buf_len - h->len) - 1;
	h->len += MultiByteToWideChar(CP_ACP, 0, value, -1, h->str + h->len, h->buf_len - h->len) - 1;
	h->len += MultiByteToWideChar(CP_ACP, 0, "\r\n", -1, h->str + h->len, h->buf_len - h->len) - 1;
	return 0;
}

LPOLESTR BuildResponseHeaderString(request_rec *r)
{
	wcheaders h;
	h.buf_len = 0;
	h.len = 0;
	// first, see how long the string will be	
	h.buf_len += MultiByteToWideChar(CP_ACP, 0, r->protocol, -1, NULL, 0);
	h.buf_len += MultiByteToWideChar(CP_ACP, 0, r->status_line, -1, NULL, 0) + 1;
	apr_table_do(__calc_header_len, &h, r->headers_out, NULL);
	__calc_header_len(&h, "Content-Type", r->content_type);
	h.buf_len += 2;

	// allocate the buffer and copy the strings
	h.str = SysAllocStringLen(NULL, h.buf_len);
	h.len += MultiByteToWideChar(CP_ACP, 0, r->protocol, -1, h.str + h.len, h.buf_len - h.len) - 1;
	h.len += MultiByteToWideChar(CP_ACP, 0, " ", -1, h.str + h.len, h.buf_len - h.len) - 1;
	h.len += MultiByteToWideChar(CP_ACP, 0, r->status_line, -1, h.str + h.len, h.buf_len - h.len) - 1;
	h.len += MultiByteToWideChar(CP_ACP, 0, "\r\n", -1, h.str + h.len, h.buf_len - h.len) - 1;
	apr_table_do(__concat_header, &h, r->headers_out, NULL);
	__concat_header(&h, "Content-Type", r->content_type);
	h.len += MultiByteToWideChar(CP_ACP, 0, "\r\n", -1, h.str + h.len, h.buf_len - h.len) - 1;

	wchar_t buffer[1024];
	wcscpy(buffer, h.str);

	return h.str;
}

LPOLESTR BuildRequestHeaderString(request_rec *r)
{
	wcheaders h;
	h.buf_len = 1;
	h.len = 0;
	// first, see how long the string will be
	apr_table_do(__calc_header_len, &h, r->headers_in, NULL);
	// allocate the buffer
	h.str = SysAllocStringLen(NULL, h.buf_len);
	apr_table_do(__concat_header, &h, r->headers_in, NULL);
	return h.str;
}

WCHAR *fragment(apr_pool_t *p, const char *s, ap_regmatch_t m) {
	if(m.rm_so < m.rm_eo) {
		return apr_pstrndup(p, s + m.rm_so, m.rm_eo - m.rm_so);
	}
	else {
		return NULL;
	}
}


bool ParseUrl(apr_pool_t *p, UrlComponents *url, const char *s)
{
	static ap_regex_t *url_parser_regex = NULL;
	const char *re = "((\\w+)://(([^@:]*)(:([^@]*))?@)?([\\w.-]+)(:(\\d{0,5}))?)?(([^\\?\\*<>:\"]*)?(\\?([^#]+))?(#(.+))?)";
	// Capturing:
	// 1 -> root
	// 2 -> scheme
	// 4 -> user
	// 6 -> password
	// 7 -> hostname
	// 9 -> port
	// 10 -> URI
	// 11 -> path
	// 13 -> query
	// 15 -> fragment

	if(!url_parser_regex) {
		url_parser_regex = ap_pregcomp(Application.Process->pool, re, 0);
	}

	ap_regmatch_t matches[16];

	if(ap_regexec(url_parser_regex, s, 16, matches, 0) == 0) {
		if(matches[1].rm_so >= 0) {

			// set these only if we have fully qualified url
			url->Scheme = fragment(p, s, matches[2]);
			url->User = fragment(p, s, matches[4]);
			url->Password = fragment(p, s, matches[6]);
			url->Host = fragment(p, s, matches[7]);

			int num = 0;
			if(matches[9].rm_so >= 0) {
				for(int i = matches[9].rm_so; i < matches[9].rm_eo; i++) {
					num = (num * 10) + (s[i] - '0');
				}
			}
			url->Port = (num) ? num : 80;
		}
		url->Relative = fragment(p, s, matches[10]);
		url->Path = fragment(p, s, matches[11]);
		url->Query = fragment(p, s, matches[13]);
		url->Fragment = fragment(p, s, matches[15]);

		if(!url->Path) {
			url->Path = apr_pstrdup(p, "/");
		}
		if(!url->Relative) {
			url->Relative = apr_pstrdup(p, "/");
		}
		if(url->Path[0] != '/') {
			url->Path = apr_pstrcat(p, "/", url->Path, NULL);
		}
		if(url->Relative[0] != '/') {
			url->Relative = apr_pstrcat(p, "/", url->Relative, NULL);
		}
	}
	else {
		// bugus URL
		return false;
	}

	// form the fully qualified url
	int len = 1;
	if(url->Scheme) {
		iovec parts[10];
		int j = 0;
		parts[j].iov_base = url->Scheme; parts[j].iov_len = strlen(url->Scheme); j++;
		parts[j].iov_base = const_cast<char*>("://"); parts[j].iov_len = 3; j++;
		if(url->User) {
			parts[j].iov_base = url->User; parts[j].iov_len = strlen(url->User); j++;
			if(url->Password) {
				parts[j].iov_base = const_cast<char*>(":"); parts[j].iov_len = 1; j++;
				parts[j].iov_base = url->Password; parts[j].iov_len = strlen(url->Password); j++;
			}
			parts[j].iov_base = const_cast<char*>("@"); parts[j].iov_len = 1; j++;
		}
		if(url->Host) {
			parts[j].iov_base = url->Host; parts[j].iov_len = strlen(url->Host); j++;
		}
		char buffer[16];
		if(url->Port != 80) {
			parts[j].iov_base = const_cast<char*>(":"); parts[j].iov_len = 1; j++;
			parts[j].iov_base = buffer; parts[j].iov_len = strlen(buffer); j++;
		}
		parts[j].iov_base = url->Relative; parts[j].iov_len = strlen(url->Relative); j++;
		url->Full = apr_pstrcatv(p, parts, j, NULL);
	}

	return true;
}

bool ParseUrl(apr_pool_t *p, UrlComponents *url, const WCHAR *s) {
	char *cstr = WideStringToPoolCStr(p, s, -1);
	return ParseUrl(p, url, cstr);
}

bool IsLonelyFileBrigade(apr_bucket_brigade *bb, const char **filename) {
	apr_bucket *first = APR_BRIGADE_FIRST(bb);
	apr_bucket *last = APR_BRIGADE_LAST(bb);
	if(APR_BUCKET_IS_FILE(first) && APR_BUCKET_IS_EOS(last)) {
		apr_bucket_file *fb = reinterpret_cast<apr_bucket_file *>(first->data);
		if(!fb->fd->pipe) {
			*filename = fb->fd->fname;
			return true;
		}
	}
	return false;
}

int chdir(const char *dir) {
	char buffer[MAX_PATH + 1];
	if(strlen(dir) > MAX_PATH) {
		return -1;
	}
	strcpy(buffer, dir);
	for(int i = 0; buffer[i] != '\0'; i++) {
		if(buffer[i] == '/') {
			buffer[i] = '\\';
		}
	}
	return SetCurrentDirectoryA(buffer) ? 0 : -1;
}

bool GetProperty(IDispatch *disp, LPOLESTR name, VARIANT *pResult, VARTYPE type) {
	HRESULT hr;
	DISPID dispId;
	hr = disp->GetIDsOfNames(IID_NULL, &name, 1, 0x0409, &dispId);
	if(hr == S_OK) {
		DISPPARAMS dispParams;
		dispParams.cArgs = 0;
		dispParams.cNamedArgs = 0;
		VARIANT ret;
		VariantInit(&ret);
        hr = disp->Invoke(dispId, IID_NULL, 0x0409, DISPATCH_PROPERTYGET, &dispParams, &ret, NULL, NULL);
		if(hr == S_OK) {
			hr = VariantChangeType(pResult, &ret, 0, type);
			VariantClear(&ret);
		}
	}
	return (hr == S_OK);
}

bool EnumMenuRecursive(HMENU hMenu, EnumMenuRecursiveProc func, void *data) {
	bool cont = true;
	int count = GetMenuItemCount(hMenu);
	for(int i = 0; i < count && cont; i++) {
		int id = GetMenuItemID(hMenu, i);
		if(id != -1) {
			cont = func(hMenu, i, id, data);
		} else {
			HMENU submenu = GetSubMenu(hMenu, i);
			if(submenu) {
				cont = EnumMenuRecursive(submenu, func, data);
			}
		}
	}
	return cont;
}

#ifdef SHOW_DEBUG_MESSAGE
void debug_print(void *this_ptr, const char *label) {
	if(!RedirectingToConsole()) {
		RedirectIOToConsole(true);
	}

	printf("[%4x] {%X} %s\n", GetCurrentThreadId(), this_ptr, label);
	fflush(stdout);
}
#endif