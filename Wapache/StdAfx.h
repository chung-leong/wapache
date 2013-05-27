// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <shellapi.h>
#include <comutil.h>
#include <ole2.h>
#include <oleauto.h>
#include <winsock2.h>
//#include <atlbase.h>
#include <exdisp.h>
#include <exdispid.h>
#include <servprov.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <mshtmcid.h>
#include <mlang.h>
#include <wininet.h>
#include <urlmon.h>
#include <shlobj.h>
#include <commdlg.h>
#include <wininet.h>
#include <wchar.h>

extern "C" 
{
#include "apr.h"
#include "apr_dso.h"
#include "apr_strings.h"
#include "apr_errno.h"
#include "apr_tables.h"
#include "apr_strings.h"
#include "apr_getopt.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_arch_file_io.h"
#include "apr_arch_networkio.h"
#include "apr_date.h"

#define CORE_PRIVATE
#include "ap_config.h"
#include "httpd.h"
#include "http_main.h"
#include "http_connection.h"
#include "http_request.h"
#include "http_protocol.h"
#include "ap_mpm.h"
#include "mpm_default.h"
#include "http_config.h"
#include "http_core.h"
#include "http_vhost.h"
#include "scoreboard.h"
#include "http_log.h"
#include "util_filter.h"
#include "mod_core.h"
}

#include "wapache.h"
#include "constants.h"
#include "util.h"
#include "wa_config.h"

extern HINSTANCE _hinstance;

#define IDM_ABOUT				104
#define IDM_EXIT				105

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
