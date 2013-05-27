#include "stdafx.h"
#include "WapacheLock.h"
#include "WapacheProtocol.h"
#include "WapacheApplication.h"
#include "WapacheDispatcher.h"

APLOG_USE_MODULE(wa_core);

/* 4BA03260-7C19-4AE9-94F6-5836D9931875 */
EXTERN_C const GUID CLSID_WapacheProtocol
	= {0x4BA03260L,0x7C19,0x4AE9,{0x94,0xF6,0xF6,0x36,0xD9,0x93,0x18,0x75}};


WapacheProtocol::WapacheProtocol(IUnknown *pUnk)
{
	RefCount = 0;
	OuterUnknown = pUnk;
	Sink = NULL;
	BindInfo = NULL;
	HttpNegotiate = NULL;
	Terminated = false;
	Redirecting = false;
	NeedFile = false;

	HttpStatusCode = 200;
	DataRead = 0;
	TotalDataLen = 0;
	Charset = "iso-8859-1";
	Browser = NULL;
	AttachmentFilename = NULL;

	BB = NULL;
	AvailData = NULL;
	AvailDataLen = NULL;

	TempFilename = NULL;
	TempFile = 0;

	DataDispatcher = NULL;
	CommandDispatcher = NULL;

	ZeroMemory(&CurrentUrl, sizeof(CurrentUrl));

	// create a pool on the heap for the transaction
    apr_pool_create(&Pool, NULL);
	Connection = NULL;
}

WapacheProtocol::~WapacheProtocol(void)
{
	if(Browser) {
		Browser->Release();
	}
	delete DataDispatcher;
	delete CommandDispatcher;
	apr_pool_destroy(Pool);
	if(TempFile) {
		CloseHandle(TempFile);
	}
	if(TempFilename) {
		DeleteFile(TempFilename);
	}
}

DWORD WINAPI WapacheProtocol::ThreadProc(LPVOID lpParameter) {

	WapacheProtocol *self = reinterpret_cast<WapacheProtocol *>(lpParameter);

	self->AddRef();
	if(Application.LockApacheConfig(READ_LOCK)) {
		self->ProcessConnection();
		Application.UnlockApacheConfig(READ_LOCK);
	}
	else {
		self->ReportResult(E_ABORT, ERROR_CANCELLED);		
	}
	self->Release();
	return 0;
}

void WapacheProtocol::ProcessConnection(void) {
	// this function runs inside the worker thread
	apr_socket_t *sock;
	long thread_num = 1;
    ap_sb_handle_t *sbh;
	apr_bucket_alloc_t *ba;
	bool redirected;

	// create a fake socket
	sock = (apr_socket_t *) apr_pcalloc(Pool, sizeof(apr_socket_t));

    sock->pool = Pool;
    sock->local_port_unknown = 1;
    sock->local_interface_unknown = 1;
    sock->remote_addr_unknown = 1;
    sock->userdata = (sock_userdata_t *) this;

    ba = apr_bucket_alloc_create(Pool);

    ap_create_sb_handle(&sbh, Pool, 0, thread_num);
	Connection = ap_run_create_connection(Pool, Application.ServerConf, sock, thread_num, sbh, ba);

	if(Connection) {
		do {
			redirected = Redirecting;
			Redirecting = false;
			if(!Terminated) {
				ap_process_connection(Connection, sock);
			}
		} while(Redirecting);
	}
	else {
		// can't create the connection object for some reason
		HttpStatusCode = 500;
	}

	if(!HttpStatusCode || HttpStatusCode >= 400 || HttpStatusCode == 204) {
		ReportResult(E_ABORT, ERROR_CANCELLED);
	}
	else {
		ReportResult(S_OK, 0);
	} 
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::QueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if(riid == IID_IUnknown) {
		if(OuterUnknown) {
			return OuterUnknown->QueryInterface(riid, ppvObject);
		}
		else {
			return NonDelegateQueryInterface(riid, ppvObject);
		}
	}
	else if(riid == IID_IInternetProtocol) {
		*ppvObject = static_cast<IInternetProtocol *>(this);
	}
	else if(riid == IID_IWinInetInfo) {
		*ppvObject = static_cast<IWinInetInfo *>(this);
	}
	else if(riid == IID_IWinInetHttpInfo) {
		*ppvObject = static_cast<IWinInetHttpInfo *>(this);
	}
	else if(riid == IID_IInternetPriority) {
		*ppvObject = static_cast<IInternetPriority *>(this);
	}
	else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}
    
ULONG STDMETHODCALLTYPE WapacheProtocol::AddRef(void)
{
	if(OuterUnknown) {
		ULONG rc = OuterUnknown->AddRef();
		return rc;
	} 
	else {
		return NonDelegateAddRef();
	}
}
    
ULONG STDMETHODCALLTYPE WapacheProtocol::Release(void)
{
	if(OuterUnknown) {
		ULONG rc = OuterUnknown->Release();
		return rc;
	}
	else {
		return NonDelegateRelease();
	}
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::NonDelegateQueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if(riid == IID_IUnknown) {
		*ppvObject = static_cast<INonDelegateUnknown *>(this);
	}
	else {
		return QueryInterface(riid, ppvObject);
	}
	NonDelegateAddRef();
	return S_OK;
}
    
ULONG STDMETHODCALLTYPE WapacheProtocol::NonDelegateAddRef(void)
{
	ULONG rc = ++RefCount;
	return rc;
}
    
ULONG STDMETHODCALLTYPE WapacheProtocol::NonDelegateRelease(void)
{
	ULONG rc;
	if(--RefCount == 0) {
		delete this;
		rc = 0;
	} 
	else {
		rc = RefCount;
	}
	return rc;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Start( 
    /* [in] */ LPCWSTR szUrl,
    /* [in] */ IInternetProtocolSink __RPC_FAR *pOIProtSink,
    /* [in] */ IInternetBindInfo __RPC_FAR *pOIBindInfo,
    /* [in] */ DWORD grfPI,
    /* [in] */ DWORD dwReserved)
{
	HRESULT hr = INET_E_INVALID_URL;

	// get the browser who made the request
	Browser = ActiveBrowser;
	ActiveBrowser = NULL;
	ReleaseMutex(SetBrowserMutex);

	// proceed only if URL is correct
	if(ParseUrl(Pool, &CurrentUrl, szUrl)) {
		Sink = pOIProtSink;
		BindInfo = pOIBindInfo;
		Sink->AddRef();
		BindInfo->AddRef();

		// see if the request is a POST or a GET
		BINDINFO bi;
		DWORD flags;
		ZeroMemory(&bi, sizeof(BINDINFO));
		bi.cbSize = sizeof(BINDINFO);
		hr = BindInfo->GetBindInfo(&flags, &bi);
		HttpMethod = (bi.dwBindVerb == BINDVERB_POST) ? M_POST : M_GET;

		if(flags & BINDF_NEEDFILE) {
			NeedFile = true;
		}

		// get the IHttpNegotiate interface
		IServiceProvider *serf = NULL;
		hr = Sink->QueryInterface(&serf);
		if(serf) {
			hr = serf->QueryService(IID_IHttpNegotiate, &HttpNegotiate);
			serf->Release();
		}

		// run the request processing code in a thread
		if(WorkThreadId = Application.RunThread(ThreadProc, (LPVOID) this)) {
			hr = E_PENDING;
		}
		else {
			hr = E_OUTOFMEMORY;
		}
	}

	return hr;
}

// invoked by IInternetProcotolSink::Switch
HRESULT STDMETHODCALLTYPE WapacheProtocol::Continue( 
    /* [in] */ PROTOCOLDATA __RPC_FAR *pProtocolData)
{
	int operation = pProtocolData->cbData;
	if(operation == REPORT_PROGRESS) {
		const char *text = reinterpret_cast<char *>(pProtocolData->pData);
		ReportProgress(pProtocolData->dwState, text);
	}
	else if(operation == REPORT_RESULT) {
		HRESULT res = pProtocolData->dwState;
		DWORD err = reinterpret_cast<HRESULT>(pProtocolData->pData);
		ReportResult(res, err);
	}
	else if(operation == REPORT_DATA) {
		apr_bucket_brigade *bb = reinterpret_cast<apr_bucket_brigade *>(pProtocolData->pData);
		ReportData(bb);
	}
	else if(operation == REPORT_RESPONSE) {
		HANDLE event = reinterpret_cast<HANDLE>(pProtocolData->dwState);
		request_rec *r = reinterpret_cast<request_rec *>(pProtocolData->pData);
		ReportResponse(r);
		SetEvent(event);
	}
	else if(operation == INSERT_HEADERS) {
		HANDLE event = reinterpret_cast<HANDLE>(pProtocolData->dwState);
		request_rec *r = reinterpret_cast<request_rec *>(pProtocolData->pData);
		InsertRequestHeaders(r);
		SetEvent(event);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Abort( 
    /* [in] */ HRESULT hrReason,
    /* [in] */ DWORD dwOptions)
{
	Terminated = true;
	if(Connection) {
		Connection->aborted = true;
	}
	if(BB) {
		apr_brigade_destroy(BB);
		BB = NULL;
		AvailData = NULL;
		AvailDataLen = 0;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Terminate( 
    /* [in] */ DWORD dwOptions)
{
	Terminated = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Suspend(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Resume(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Read( 
    /* [length_is][size_is][out][in] */void __RPC_FAR *pv,
    /* [in] */ ULONG cb,
    /* [out] */ ULONG __RPC_FAR *pcbRead)
{
	HRESULT hr = S_OK;
	ULONG space = cb;
	char *pvb = reinterpret_cast<char *>(pv);
	*pcbRead = 0;
	while (space != 0 && BB && !APR_BRIGADE_EMPTY(BB)) {
		apr_bucket *first = APR_BRIGADE_FIRST(BB);
		if(!AvailData) {
			apr_status_t rv;
			// Apache2 uses memory-map for static files, so the whole thing will be available
			rv = apr_bucket_read(first, &AvailData, &AvailDataLen, APR_NONBLOCK_READ);
			if(APR_STATUS_IS_TIMEUP(rv)) {
				break;
			}
		}
		int copy = min(space, AvailDataLen);
		if(copy > 0) {
			memcpy(pvb, AvailData, copy);
			pvb += copy;
			*pcbRead += copy;
			space -= copy;
			if(TempFile) {
				DWORD written = 0;
				WriteFile(TempFile, AvailData, copy, &written, NULL);
			}
			AvailData += copy;
			AvailDataLen -= copy;
			DataRead += copy;
		}
		if(AvailDataLen == 0) {
			if(APR_BUCKET_IS_EOS(first)) {
				// end-of-stream bucket;
				apr_brigade_destroy(BB);
				BB = NULL;
				if(TempFile) {
					CloseHandle(TempFile);
					TempFile = 0;
				}
			} else {
				// throw out the bucket
				apr_bucket_delete(first);
			}
			AvailData = NULL;
			AvailDataLen = 0;
		}
	}
	if(!*pcbRead) {
		// nothing read--there could be more stuff coming however
		hr = E_PENDING;
	}
	if(!BB) {
		if(!*pcbRead) {
			// no more data
			hr = S_FALSE;
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::Seek( 
    /* [in] */ LARGE_INTEGER dlibMove,
    /* [in] */ DWORD dwOrigin,
    /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::LockRequest( 
    /* [in] */ DWORD dwOptions)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::UnlockRequest(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::QueryOption( 
    /* [in] */ DWORD dwOption,
    /* [size_is][out][in] */ LPVOID pBuffer,
    /* [out][in] */ DWORD *pcbBuf)
{
	HRESULT result = S_OK;
	unsigned long *pUlong = reinterpret_cast<unsigned long *>(pBuffer);
	switch(dwOption) {
		case INTERNET_OPTION_SECURITY_FLAGS:
			*pUlong = 0;
			break;
		case INTERNET_OPTION_REQUEST_FLAGS:
			*pUlong = INTERNET_REQFLAG_CACHE_WRITE_DISABLED;
			break;
		default:
			result = E_NOTIMPL;
	}
	return result;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::QueryInfo( 
    /* [in] */ DWORD dwOption,
    /* [size_is][out][in] */ LPVOID pBuffer,
    /* [out][in] */ DWORD *pcbBuf,
    /* [out][in] */ DWORD *pdwFlags,
    /* [out][in] */ DWORD *pdwReserved)
{
	HRESULT result = S_OK;
	DWORD *pDword = reinterpret_cast<DWORD *>(pBuffer);
	char *pChar = reinterpret_cast<char *>(pBuffer);
	SYSTEMTIME *pSystemTime = reinterpret_cast<SYSTEMTIME *>(pBuffer);
	DWORD dwFlags = HTTP_QUERY_MODIFIER_FLAGS_MASK & dwOption;
	dwOption &= ~HTTP_QUERY_MODIFIER_FLAGS_MASK;
	switch(dwOption) {
		case HTTP_QUERY_STATUS_CODE:
			*pDword = HttpStatusCode;
			break;
		case HTTP_QUERY_CONTENT_TYPE:
			apr_cpystrn(pChar, ContentType, *pcbBuf);
			*pcbBuf = min(strlen(ContentType) + 1, *pcbBuf);
			break;
		case HTTP_QUERY_LAST_MODIFIED:
			CopyMemory(pSystemTime, &LastModifiedTime, sizeof(LastModifiedTime));
			break;
		default:
			result = E_NOTIMPL;
	}
	return result;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::SetPriority( 
		/* [in] */ LONG nPriority)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocol::GetPriority( 
		/* [out] */ LONG *pnPriority)
{
	return S_OK;
}

void WapacheProtocol::ReportProgress(DWORD statusCode, const char *text) {
	if(Terminated) {
		return;
	}
	if(InWorkerThread()) {
		PROTOCOLDATA pd;
		pd.cbData = REPORT_PROGRESS;
		pd.grfFlags = PD_FORCE_SWITCH;
		pd.dwState = statusCode;
		pd.pData = apr_pstrdup(Pool, text ? text : "");

		Sink->Switch(&pd);
	}
	else {
		LPOLESTR textW = CStrToWideString(text, -1);
		if(statusCode == BINDSTATUS_CACHEFILENAMEAVAILABLE) {
			for(int i = 0; textW[i] != '\0'; i++) {
				if(textW[i] == '/') {
					textW[i] = '\\';
				}
			}
		}
		Sink->ReportProgress(statusCode, textW);
		SysFreeString(textW);
	}
}

void WapacheProtocol::ReportResult(HRESULT result, DWORD errorCode) {
	if(InWorkerThread()) {
		PROTOCOLDATA pd;
		pd.cbData = REPORT_RESULT;
		pd.grfFlags = PD_FORCE_SWITCH;
		pd.dwState = result;
		pd.pData = reinterpret_cast<LPVOID>(errorCode);
		Sink->Switch(&pd);
	}
	else {
		// ReportResult is always the last call to Sink
		Sink->ReportResult(result, errorCode, NULL);

		// need to release these objects to release the references to the protocol object
		Sink->Release();
		BindInfo->Release();
		HttpNegotiate->Release();
	}
}

#define BINDSTATUS_CONTENTDISPOSITIONFILENAME 49

void WapacheProtocol::ReportData(apr_bucket_brigade *bb) {
	if(Terminated) {
		return;
	}
	if(InWorkerThread()) {
		PROTOCOLDATA pd;
		pd.cbData = REPORT_DATA;
		pd.grfFlags = PD_FORCE_SWITCH;
		pd.dwState = 0;
		pd.pData = bb;
		Sink->Switch(&pd);
	}
	else {
		// report the data availability now that we'll in the apartment thread
		if(!BB) {
			BB = bb;
		}
		else {
			// move the buckets into the existing brigade
			APR_BRIGADE_CONCAT(BB, bb);
			apr_brigade_destroy(bb);
		}

		apr_bucket *last = APR_BRIGADE_LAST(BB);

		DWORD flags = 0;
		if(TotalDataLen == 0) {
			flags |= BSCF_AVAILABLEDATASIZEUNKNOWN;
		}
		if(DataRead == 0) {
			flags |= BSCF_FIRSTDATANOTIFICATION;
		}
		else {
			flags |= BSCF_INTERMEDIATEDATANOTIFICATION;
		}
		if(APR_BUCKET_IS_EOS(last)) {
			flags |= BSCF_LASTDATANOTIFICATION;
		}
		if(flags == (BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION)) {
			flags = BSCF_DATAFULLYAVAILABLE | BSCF_LASTDATANOTIFICATION;
		}
		if(NeedFile && HttpStatusCode >= 200 && HttpStatusCode < 299) {
			const char *filename;
			if(IsLonelyFileBrigade(BB, &filename)) {
				// destroy the bucket brigade, so ::Read doesn't waste time reading stuff that won't be used anyway
				apr_brigade_destroy(BB);
				BB = NULL;
				DataRead = TotalDataLen;
				ReportProgress(BINDSTATUS_CACHEFILENAMEAVAILABLE, filename);
			} else {
				// need to write to a temp file
				if(!TempFile) {
					char temp_path[MAX_PATH];
					GetTempPath(MAX_PATH, temp_path);
					TempFilename = (char *) apr_palloc(Pool, MAX_PATH + 1);
					if(AttachmentFilename) {
						sprintf(TempFilename, "%s\\%s", temp_path, AttachmentFilename);
					} else {
						GetTempFileName(temp_path, "wapache", 0, TempFilename);
					}
					TempFile = CreateFile(TempFilename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, 0);
					ReportProgress(BINDSTATUS_CACHEFILENAMEAVAILABLE, TempFilename);
				}
			}
		}
		if(AttachmentFilename) {
			ReportProgress(BINDSTATUS_CONTENTDISPOSITIONFILENAME, AttachmentFilename);
			ReportProgress(BINDSTATUS_CONTENTDISPOSITIONATTACH, AttachmentFilename);
		}

		// always report that we have everything
		Sink->ReportData(flags, TotalDataLen, TotalDataLen);
	}
}

void WapacheProtocol::ReportResponse(request_rec *r) {
	if(Terminated) {
		return;
	}
	if(InWorkerThread()) {
		HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
		PROTOCOLDATA pd;
		pd.grfFlags = PD_FORCE_SWITCH;
		pd.cbData = REPORT_RESPONSE;
		pd.dwState = reinterpret_cast<DWORD>(event);
		pd.pData = r;

		if(SUCCEEDED(Sink->Switch(&pd))) {
			WaitForSingleObject(event, INFINITE);
		}
		CloseHandle(event);
	}
	else {
		LPOLESTR url = NULL;
		LPOLESTR headers = BuildResponseHeaderString(r); 
		HttpNegotiate->OnResponse(HttpStatusCode, headers, NULL, NULL);
		SysFreeString(url);
		SysFreeString(headers);
	}
}

void WapacheProtocol::InsertRequestHeaders(request_rec *r) {
	if(Terminated) {
		return;
	}
	if(InWorkerThread()) {
		HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
		PROTOCOLDATA pd;
		pd.grfFlags = PD_FORCE_SWITCH;
		pd.cbData = INSERT_HEADERS;
		pd.dwState = reinterpret_cast<DWORD>(event);
		pd.pData = r;

		if(SUCCEEDED(Sink->Switch(&pd))) {
			WaitForSingleObject(event, INFINITE);
		}
		CloseHandle(event);
	}
	else {
		// headers from IInternetBindInfo
		InsertBindingHeader(r, "Accept", BINDSTRING_ACCEPT_MIMES);
		InsertBindingHeader(r, "User-Agent", BINDSTRING_USER_AGENT);
		InsertBindingHeader(r, "Content-Type", BINDSTRING_POST_DATA_MIME);

		// headers from IHttpNegotiate (i.e. MSHTML)
		LPOLESTR url = CStrToWideString(CurrentUrl.Full, -1);
		LPOLESTR headers = BuildRequestHeaderString(r); 
		LPOLESTR additional_headers = NULL;
		HttpNegotiate->BeginningTransaction(url, headers, NULL, &additional_headers);
		
		// some sort of race condition inside XHttpRequest
		Sleep(0);
		InsertAdditionalHeaders(r, additional_headers);
		SysFreeString(url);
		SysFreeString(headers);
		CoTaskMemFree(additional_headers);
	}
}

bool WapacheProtocol::BindApacheRequest(request_rec *r)
{
	if(Terminated) {
		return false;
	}
	// fill in fields concerning the request
	r->protocol = "HTTP/1.1";
	r->unparsed_uri = CurrentUrl.Relative;
	r->uri = CurrentUrl.Path;
	r->args = CurrentUrl.Query;
	r->method_number = HttpMethod;
	r->method = (HttpMethod == M_POST) ? "POST" : "GET";
	
	r->the_request = apr_pstrcat(r->pool, r->method, " ", r->unparsed_uri, " ", r->protocol, NULL);

	// add headers for request from BindInfo and HttpNegotiate
	InsertRequestHeaders(r);

	// add Host header for virtual hosting
	apr_table_add(r->headers_in, "Host", CurrentUrl.Host);

	// stick in cookie
	const char *cookie;
	Application.LockCookieTable(READ_LOCK);
	cookie = Application.GetCookie(CurrentUrl.Host);
	Application.UnlockCookieTable(READ_LOCK);
	apr_table_addn(r->headers_in, "Cookie", cookie);

	// tell Sink that sending the request
	ReportProgress(BINDSTATUS_SENDINGREQUEST, NULL);

	RestoreEnvironment(r);
	return true;
}

void WapacheProtocol::InsertBindingHeader(request_rec *r, const char *key, ULONG ulStringType) 
{
	HRESULT hr;
	LPOLESTR bindStr[32];
	ULONG count = 32;
	hr = BindInfo->GetBindString(ulStringType, bindStr, count, &count);
	if(SUCCEEDED(hr) && count > 0) {
		// get the length of the strings concatenated
		char *value = WideStringsToPoolCStr(r->pool, const_cast<LPCWSTR *>(bindStr), count, ";");
		if(value) {
			apr_table_addn(r->headers_in, key, value);
			hr = S_OK;
		}
		for(ULONG i = 0; i < count; i++) {
			CoTaskMemFree(bindStr[i]);
		}
	}
}

void WapacheProtocol::InsertAdditionalHeaders(request_rec *r, LPOLESTR szHeaders) 
{
	if(szHeaders) {
		LPCWSTR nameEndW, valueEndW;
		LPCWSTR nameW, valueW;
		LONG nameLen, valueLen;

		nameW = szHeaders;
		while(nameEndW = wcsstr(nameW, L":")) {
			nameLen = WideStringFragmentLen(nameW, nameEndW);

			valueW = nameEndW + 1;
			valueEndW = wcsstr(valueW, L"\n");
			if(!valueEndW) {
				valueEndW = szHeaders + wcslen(szHeaders);
			}
			valueLen = WideStringFragmentLen(valueW, valueEndW);

			WideStringTrim(&nameW, &nameLen);
			WideStringTrim(&valueW, &valueLen);

			char *name = WideStringToPoolCStr(r->pool, nameW, nameLen);
			char *value = WideStringToPoolCStr(r->pool, valueW, valueLen);

			apr_table_addn(r->headers_in, name, value);

			nameW = valueEndW + 1;
		}
	}
}

apr_bucket *WapacheProtocol::CreateInputBucket(apr_bucket_alloc_t *list)
{
	if(HttpMethod == M_POST) {
		BINDINFO bi;
		DWORD flags;
		ZeroMemory(&bi, sizeof(BINDINFO));
		bi.cbSize = sizeof(BINDINFO);
		BindInfo->GetBindInfo(&flags, &bi);
		if(bi.dwBindVerb == BINDVERB_POST) {
			HGLOBAL glob = NULL;
			apr_size_t len = 0;
			if(bi.stgmedData.tymed == TYMED_HGLOBAL) {
				// POST data is passed as a global memory handle
				glob = bi.stgmedData.hGlobal;
			}
			else if(bi.stgmedData.tymed == TYMED_ISTREAM) {
				// POST data is passed as an IStream
				// create a memory stream add copy everything over
				IStream *in_stream = bi.stgmedData.pstm;
				IStream *out_stream = NULL;
				CreateStreamOnHGlobal(NULL, false, &out_stream);
				if(out_stream) {
					ULARGE_INTEGER max, copied;
					max.HighPart = 0;
					max.LowPart = 0xFFFFFFFF;
					in_stream->CopyTo(out_stream, max, NULL, &copied);
					GetHGlobalFromStream(out_stream, &glob);
					len = copied.LowPart;
					out_stream->Release();
				}				
			}
			if(glob) {
				// lock the memory block and stick it into a heap bucket
				void *p = GlobalLock(glob);
				const char *data = reinterpret_cast<const char *>(p);
				len = bi.cbstgmedData;
				return apr_bucket_heap_create(data, len, free_global_mem, list);
			}
		}
	}
	// a GET request = end-of-stream bucket
	return apr_bucket_eos_create(list);
}

// process response headers from Apache
bool WapacheProtocol::ReportApacheResponseStart(request_rec *r) 
{
	// see if we need to redirect
	HRESULT hr = S_OK;
	if(r->status == 301 || r->status == 302 || r->status == 303 || r->status == 307) {
		const char *location = apr_table_get(r->headers_out, "Location");
		if(location) {
			if(ParseUrl(r->connection->pool, &CurrentUrl, location)) {
				ReportProgress(BINDSTATUS_REDIRECTING, CurrentUrl.Full);
				Redirecting = true;
				if(r->status == 302 || r->status == 303) {
					// 302 and 303 redirects are always GET
					HttpMethod = M_GET;
				}
			}
			else {
				HttpStatusCode = 400;
			}
		}
	}

	const char *s = apr_table_get(r->headers_out, "Content-type");
	ContentType = apr_pstrdup(Pool, s ? s : "");
	s = apr_table_get(r->headers_out, "Last-Modified");
	apr_time_t t = (s) ? apr_date_parse_http(s) : 0;
	ULARGE_INTEGER large_int;
	large_int.QuadPart = (t * 10) + 116444736000000000ULL;
	FILETIME f;
	f.dwHighDateTime = large_int.HighPart;
	f.dwLowDateTime = large_int.LowPart;
	SYSTEMTIME st;
	st.wMilliseconds = 0;
	st.wSecond = 0;
	st.wMinute = 0;
	st.wHour = 0;
	st.wMonth = 1;
	st.wDay = 1;
	st.wYear = 1;
	//SystemTimeToFileTime(&st, &f);
	FileTimeToSystemTime(&f, &LastModifiedTime);

	strlwr(ContentType);
	s = strstr(ContentType, "charset=");
	if(s) {
		Charset = s + 8;
	}

	// save cookie
	const char *cookie = apr_table_get(r->headers_out, "Set-Cookie");
	if(cookie) {
		Application.LockCookieTable(WRITE_LOCK);
		Application.SetCookie(CurrentUrl.Host, cookie);
		Application.UnlockCookieTable(WRITE_LOCK);
	}

	ProcessCommandHeaders(r, "Before");

	const char *lineHandler = apr_table_get(r->headers_out, "X-Line-Handler");
	if(lineHandler) {
		HttpStatusCode = 204;

		const char *p = lineHandler;
		const char *winName = ap_getword_conf(r->pool, &p);
		const char *method = ap_getword_conf(r->pool, &p);

		if(winName[0] != '\0' && method[0] != '\0') {
			const char *pattern = apr_pstrcat(r->pool, "^", winName, "$", NULL);
			ap_regex_t *re = ap_pregcomp(r->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
			if(!re) {
		        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
							  "Cannot compile pattern '%s' in X-Line-Handler header");
				return false;
			}
			DataDispatcher = new WapacheDataDispatcher(re, method, CurrentUrl.Host, Charset, Browser);
		}
		else {
	        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
		                 "Missing parameters in X-Line-Handler header");
			return false;
		}
	}
	else {
		// return the actual status code
		HttpStatusCode = r->status;
	}

	if(HttpStatusCode != 204 && !Redirecting) {
		const char *header;

		// tell the Sink that we have the MIME type
		if(ContentType) {
			ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, ContentType);
		}

		TotalDataLen = (ULONG) r->clength;

		header = apr_table_get(r->headers_out, "Content-Disposition");
		if(header) {
			static ap_regex_t *attachment_regex = NULL;
			static ap_regex_t *filename_regex = NULL;

			if(!attachment_regex) {
				const char *re = "^\\s*attachment";
				attachment_regex = ap_pregcomp(Application.Process->pool, re, AP_REG_EXTENDED | AP_REG_ICASE);
			}
			if(!filename_regex) {
				const char *re = "filename\\s*=\\s*\"?([^\"]*)\"?";
				filename_regex = ap_pregcomp(Application.Process->pool, re, AP_REG_EXTENDED | AP_REG_ICASE);
			}
			if(ap_regexec(attachment_regex, header, 0, NULL, 0) == 0) {
				ap_regmatch_t matches[2];
				if(ap_regexec(filename_regex, header, 2, matches, 0) == 0) {
					if(matches[1].rm_so < matches[1].rm_eo) {
						AttachmentFilename = apr_pstrndup(Pool, header + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
						NeedFile = true;
					}
				}
			}
		}

		// notify MSHTML that we have a response
		ReportResponse(r);
	}

	return SUCCEEDED(hr);
}

// save environment variables and process X-Command-After
bool WapacheProtocol::ReportApacheResponseEnd(request_rec *r) {
	SaveEnvironment(r);
	ProcessCommandHeaders(r, "After");
	return true;
}

const char *WapacheProtocol::CreateMenuItem(request_rec *r, const char *args, wa_menu_item_config **pItem) 
{
	const char *op = ap_getword_conf(r->pool, &args);
	const char *winName = ap_getword_conf(r->pool, &args);
	wa_menu_item_config *item = NULL;

	if(stricmp(op, "Basic") == 0) {
		const char *item_id = ap_getword_conf(r->pool, &args);
		if(item_id[0] == '\0') {
			return apr_psprintf(r->pool, "Missing command identifier in X-Command header", NULL);
		}
		int idm = CONSTANT_VALUE(MSHTML, item_id);
		if(!idm) {
			return apr_psprintf(r->pool, "Unrecognized command identifier '%s' in X-Command header", item_id, NULL);
		}

		wa_std_menu_item_config *sItem = (wa_std_menu_item_config *) apr_palloc(r->pool, sizeof(wa_std_menu_item_config));
		sItem->Type = STD_MENU_ITEM;
		sItem->CommandId = idm;
		item = sItem;
	}
	else if(stricmp(op, "Script") == 0) {
		const char *method = ap_getword_conf(r->pool, &args);
		if(method[0] == '\0') {
			return apr_psprintf(r->pool, "Missing script method name in X-Command header", NULL);
		}

		wa_js_menu_item_config *jItem = (wa_js_menu_item_config *) apr_palloc(r->pool, sizeof(wa_js_menu_item_config));
		jItem->Method = method;
		const char *a;
		while (*(a = ap_getword_conf(r->pool, &args)) != '\0') {
			if(!jItem->Args) {
				jItem->Args = apr_array_make(r->pool, 5, sizeof(const char *));
			}
			const char **arg_slot = (const char **) apr_array_push(jItem->Args);
			*arg_slot = a;
		}
		item = jItem;
	}
	else if(stricmp(op, "Url") == 0) {
		const char *url = ap_getword_conf(r->pool, &args);
		const char *target = ap_getword_conf(r->pool, &args);

		if(url[0] == '\0') {
			return apr_psprintf(r->pool, "No URL specified in X-Command header", NULL);
		}

		wa_url_menu_item_config *uItem = (wa_url_menu_item_config *) apr_palloc(r->pool, sizeof(wa_url_menu_item_config));
		uItem->Type = URL_MENU_ITEM;
		uItem->Url = url;
		uItem->Target = (target[0] != '\0') ? target : NULL;
		item = uItem;
	}
	else if(stricmp(op, "Dom") == 0) {
		const char *elem_id = ap_getword_conf(r->pool, &args);
		const char *prop = ap_getword_conf(r->pool, &args);
		const char *prop_val = ap_getword_conf(r->pool, &args);

		if(elem_id[0] == '\0') {
			return apr_psprintf(r->pool, "Missing element id in X-Command header", NULL);
		}
		if(prop[0] == '\0') {
			return apr_psprintf(r->pool, "Missing property name in X-Command header", NULL);
		}
		if(prop_val[0] == '\0') {
			return apr_psprintf(r->pool, "Missing property value in X-Command header", NULL);
		}

		wa_dom_menu_item_config *dItem = (wa_dom_menu_item_config *) apr_palloc(r->pool, sizeof(wa_dom_menu_item_config));
		dItem->Type = DOM_MENU_ITEM;
		dItem->ElemId = elem_id;
		dItem->PropName = prop;
		dItem->PropVal = prop_val;
		item = dItem;
	}
	else if(stricmp(op, "ScriptEnv") == 0) {
		const char *env_name = ap_getword_conf(r->pool, &args);
		const char *env_val = ap_getword_conf(r->pool, &args);
		const char *method = ap_getword_conf(r->pool, &args);

		if(env_name[0] == '\0') {
			return apr_psprintf(r->pool, "Missing environment variable name in X-Command header", NULL);
		}
		if(env_val[0] == '\0') {
			return apr_psprintf(r->pool, "No value given for environment variable in X-Command header", NULL);
		}
		if(method[0] == '\0') {
			return apr_psprintf(r->pool, "Missing script method name in X-Command header", NULL);
		}

		wa_js_env_menu_item_config *jItem = (wa_js_env_menu_item_config *) apr_palloc(r->pool, sizeof(wa_js_env_menu_item_config));
		jItem->Method = method;
		jItem->EnvVarName = env_name;
		jItem->EnvVarVal = env_val;
		const char *a;
		while (*(a = ap_getword_conf(r->pool, &args)) != '\0') {
			if(!jItem->Args) {
				jItem->Args = apr_array_make(r->pool, 5, sizeof(const char *));
			}
			const char **arg_slot = (const char **) apr_array_push(jItem->Args);
			*arg_slot = a;
		}
		item = jItem;
	}
	else if(stricmp(op, "UrlEnv") == 0) {
		const char *env_name = ap_getword_conf(r->pool, &args);
		const char *env_val = ap_getword_conf(r->pool, &args);
		const char *url = ap_getword_conf(r->pool, &args);
		const char *target = ap_getword_conf(r->pool, &args);

		if(env_name[0] == '\0') {
			return apr_psprintf(r->pool, "Missing environment variable name in X-Command header", NULL);
		}
		if(env_val[0] == '\0') {
			return apr_psprintf(r->pool, "No value given for environment variable in X-Command header", NULL);
		}
		if(url[0] == '\0') {
			return apr_psprintf(r->pool, "No URL specified in X-Command header", NULL);
		}

		wa_url_env_menu_item_config *uItem = (wa_url_env_menu_item_config *) apr_palloc(r->pool, sizeof(wa_url_env_menu_item_config));
		uItem->Type = URL_MENU_ITEM;
		uItem->EnvVarName = env_name;
		uItem->EnvVarVal = env_val;
		uItem->Url = url;
		uItem->Target = (target[0] != '\0') ? target : NULL;
		item = uItem;
	}
	else if(stricmp(op, "DomEnv") == 0) {
		const char *env_name = ap_getword_conf(r->pool, &args);
		const char *env_val = ap_getword_conf(r->pool, &args);
		const char *elem_id = ap_getword_conf(r->pool, &args);
		const char *prop = ap_getword_conf(r->pool, &args);
		const char *prop_val = ap_getword_conf(r->pool, &args);

		if(env_name[0] == '\0') {
			return apr_psprintf(r->pool, "Missing environment variable name in X-Command header", NULL);
		}
		if(env_val[0] == '\0') {
			return apr_psprintf(r->pool, "No value given for environment variable in X-Command header", NULL);
		}
		if(elem_id[0] == '\0') {
			return apr_psprintf(r->pool, "Missing element id in X-Command header", NULL);
		}
		if(prop[0] == '\0') {
			return apr_psprintf(r->pool, "Missing property name in X-Command header", NULL);
		}
		if(prop_val[0] == '\0') {
			return apr_psprintf(r->pool, "Missing property value in X-Command header", NULL);
		}

		wa_dom_env_menu_item_config *dItem = (wa_dom_env_menu_item_config *) apr_palloc(r->pool, sizeof(wa_dom_env_menu_item_config));
		dItem->Type = DOM_MENU_ITEM;
		dItem->EnvVarName = env_name;
		dItem->EnvVarVal = env_val;
		dItem->ElemId = elem_id;
		dItem->PropName = prop;
		dItem->PropVal = prop_val;
		item = dItem;
	}
	else {
		return apr_psprintf(r->pool, "Invalid command '%s' in X-Command header", op, NULL);
	}

	const char *pattern = apr_pstrcat(r->pool, "^", winName, "$", NULL);
	ap_regex_t *re = ap_pregcomp(r->pool, pattern, AP_REG_EXTENDED | AP_REG_ICASE);
	if(!re) {
		return apr_psprintf(r->pool, "Cannot compile pattern '%s' ub X-Command header", pattern);
	}
	item->Id = 0;
	item->Label = NULL;
	item->Pattern = re;
	*pItem = item;
	return NULL;
}

void WapacheProtocol::ProcessCommandHeaders(request_rec *r, const char *type) {
	char commandHeader[32];
	const char *command;
	int num = 0;
	while(sprintf(commandHeader, "X-Command-%s-%d", type, ++num), 
		command = apr_table_get(r->headers_out, commandHeader), command) {
		wa_menu_item_config *item;
		const char *err = CreateMenuItem(r, command, &item);

		if(!err) {
			if(!CommandDispatcher) {
				CommandDispatcher = new WapacheCommandDispatcher(CurrentUrl.Host, Charset, Browser);
			}
			CommandDispatcher->Process(item);
		}
		else {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, err);
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot process '%s'", command);
		}
	}
}

bool WapacheProtocol::TakeOutputBuckets(apr_bucket_brigade *b) {
	// don't take buckets if we're Redirecting or if there's no content
	if(HttpStatusCode != 204 && !Redirecting) {
		// create a new brigade and grab all the buckets from b
		apr_bucket_brigade *a = apr_brigade_create(Pool, b->bucket_alloc);
		apr_bucket *e;
		APR_BRIGADE_CONCAT(a, b);

		// make any transcient bucket more permanent as they'll be needed later
		for(e = APR_BRIGADE_FIRST(a); e != APR_BRIGADE_SENTINEL(a); e = APR_BUCKET_NEXT(e)) {
			apr_bucket_setaside(e, Pool);
		}
		
		// switch from the worker thread to the main thread
		ReportData(a);
	}
	else if(DataDispatcher) {
		// let the dispatcher process the brigade
		if(!DataDispatcher->Process(b)) {
			Connection->aborted = true;
		}
	}
	return true;
}

bool WapacheProtocol::RestoreEnvironment(request_rec *r)
{
    void *sconf = r->server->module_config;
    wa_core_server_config *conf = 
			(wa_core_server_config *) ap_get_module_config(sconf, &core_module);

	const char **keys;
	int keyCount;

	Application.LockApacheEnv(READ_LOCK);
	keys = (const char **) conf->sess_env->elts;
	keyCount = conf->sess_env->nelts;
	for(int i = 0; i < keyCount; i++) {
		const char *val = Application.GetEnvironmentVar(CurrentUrl.Host, keys[i], false);
		if(!val) {
			val = apr_table_get(conf->def_env, keys[i]);
		}
		if(val) {
			apr_table_set(r->subprocess_env, keys[i], val);
		}
	}
	keys = (const char **) conf->sav_env->elts;
	keyCount = conf->sav_env->nelts;
	for(int i = 0; i < keyCount; i++) {
		const char *val = Application.GetEnvironmentVar(CurrentUrl.Host, keys[i], true);
		if(!val) {
			val = apr_table_get(conf->def_env, keys[i]);
		}
		if(val) {
			apr_table_set(r->subprocess_env, keys[i], val);
		}
	}
	Application.UnlockApacheEnv(READ_LOCK);

	r->subprocess_env = apr_table_overlay(r->pool, r->subprocess_env, Application.ProgramEnvVars);
	return true;
}

void WapacheProtocol::SetBrowser(IDispatch *disp)
{
	// Start will release the mutex once it has the reference to the browser
	if(!SetBrowserMutex) {
		SetBrowserMutex = CreateMutex(NULL, TRUE, NULL);
	}
	else {
		if(WaitForSingleObject(SetBrowserMutex, 5000) == WAIT_TIMEOUT) {
			bool hung = true;
		}
	}
	disp->AddRef();
	ActiveBrowser = disp;
}

HANDLE WapacheProtocol::SetBrowserMutex;
IDispatch *WapacheProtocol::ActiveBrowser;

bool WapacheProtocol::SaveEnvironment(request_rec *r)
{
    void *sconf = r->server->module_config;
    wa_core_server_config *conf = (wa_core_server_config *) ap_get_module_config(sconf, &core_module);
	const char **keys;
	int keyCount;

	Application.LockApacheEnv(WRITE_LOCK);
	keys = (const char **) conf->sess_env->elts;
	keyCount = conf->sess_env->nelts;
	for(int i = 0; i < keyCount; i++) {
		const char *val = apr_table_get(r->subprocess_env, keys[i]);
		if(val) {
			Application.SetEnvironmentVar(CurrentUrl.Host, keys[i], val, false);
		}
	}
	keys = (const char **) conf->sav_env->elts;
	keyCount = conf->sav_env->nelts;
	for(int i = 0; i < keyCount; i++) {
		const char *val = apr_table_get(r->subprocess_env, keys[i]);
		if(val) {
			Application.SetEnvironmentVar(CurrentUrl.Host, keys[i], val, true);
		}
	}
	Application.UnlockApacheEnv(WRITE_LOCK);
	return true;
}

WapacheProtocolFactory ProtocolFactoryInstance;

HRESULT STDMETHODCALLTYPE WapacheProtocolFactory::QueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if(riid == IID_IUnknown) {
		*ppvObject = static_cast<IUnknown *>(this);
	}
	else if(riid == IID_IClassFactory) {
		*ppvObject = static_cast<IClassFactory *>(this);
	}
	else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}
    
ULONG STDMETHODCALLTYPE WapacheProtocolFactory::AddRef(void)
{
	return 1;
}
    
ULONG STDMETHODCALLTYPE WapacheProtocolFactory::Release(void)
{
	return 1;
}

/***** IClassFactory methods *****/
HRESULT STDMETHODCALLTYPE WapacheProtocolFactory::CreateInstance( 
    /* [unique][in] */ IUnknown __RPC_FAR *pUnkOuter,
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	HRESULT hr;

	WapacheProtocol *protocol = new WapacheProtocol(pUnkOuter);
	if(!protocol) {
		return E_OUTOFMEMORY;
	}
	protocol->NonDelegateAddRef();
	if(pUnkOuter) {
		hr = protocol->NonDelegateQueryInterface(riid, ppvObject);
	}
	else {
		hr = protocol->QueryInterface(riid, ppvObject);
	}
	protocol->NonDelegateRelease();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheProtocolFactory::LockServer( 
    /* [in] */ BOOL fLock)
{
	return S_OK;
}
