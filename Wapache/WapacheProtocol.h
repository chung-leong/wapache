#ifndef __WAPACHEPROTOCOL_H
#define __WAPACHEPROTOCOL_H

class INonDelegateUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE NonDelegateQueryInterface( 
	        /* [in] */ REFIID riid,
		    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject) = 0;        
    virtual ULONG STDMETHODCALLTYPE NonDelegateAddRef(void) = 0;
    virtual ULONG STDMETHODCALLTYPE NonDelegateRelease(void) = 0;
}; 

struct BrowserUrlRecord {
	BrowserUrlRecord *Next;
	BrowserUrlRecord *Prev;
	IDispatch *BrowserDisp;
	LPOLESTR Url;
};

class WapacheDataDispatcher;
class WapacheCommandDispatcher;

class WapacheProtocol :
	public INonDelegateUnknown,
	public IInternetProtocol,
	public IWinInetHttpInfo,
	public IInternetPriority
{
public:
	WapacheProtocol(IUnknown *pUnk);
	~WapacheProtocol(void);

	/***** IUnknown methods *****/
    HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
    ULONG STDMETHODCALLTYPE AddRef(void);
        
    ULONG STDMETHODCALLTYPE Release(void);

	/***** IInternetProtocol methods *****/
    HRESULT STDMETHODCALLTYPE Start( 
        /* [in] */ LPCWSTR szUrl,
        /* [in] */ IInternetProtocolSink __RPC_FAR *pOIProtSink,
        /* [in] */ IInternetBindInfo __RPC_FAR *pOIBindInfo,
        /* [in] */ DWORD grfPI,
        /* [in] */ DWORD dwReserved);
    
    HRESULT STDMETHODCALLTYPE Continue( 
        /* [in] */ PROTOCOLDATA __RPC_FAR *pProtocolData);
    
    HRESULT STDMETHODCALLTYPE Abort( 
        /* [in] */ HRESULT hrReason,
        /* [in] */ DWORD dwOptions);
    
    HRESULT STDMETHODCALLTYPE Terminate( 
        /* [in] */ DWORD dwOptions);
    
    HRESULT STDMETHODCALLTYPE Suspend(void);
    
    HRESULT STDMETHODCALLTYPE Resume(void);

    HRESULT STDMETHODCALLTYPE Read( 
        /* [length_is][size_is][out][in] */void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbRead);
    
    HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition);
    
    HRESULT STDMETHODCALLTYPE LockRequest( 
        /* [in] */ DWORD dwOptions);
    
    HRESULT STDMETHODCALLTYPE UnlockRequest(void);

	/***** INonDelegateUnknown methods *****/
    HRESULT STDMETHODCALLTYPE NonDelegateQueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
    ULONG STDMETHODCALLTYPE NonDelegateAddRef(void);
        
    ULONG STDMETHODCALLTYPE NonDelegateRelease(void);

	/***** IWinInetHttpInfo *****/
    HRESULT STDMETHODCALLTYPE QueryOption( 
        /* [in] */ DWORD dwOption,
        /* [size_is][out][in] */ LPVOID pBuffer,
        /* [out][in] */ DWORD *pcbBuf);

    HRESULT STDMETHODCALLTYPE QueryInfo( 
        /* [in] */ DWORD dwOption,
        /* [size_is][out][in] */ LPVOID pBuffer,
        /* [out][in] */ DWORD *pcbBuf,
        /* [out][in] */ DWORD *pdwFlags,
        /* [out][in] */ DWORD *pdwReserved);

	/***** IInternetPriority  *****/
	HRESULT STDMETHODCALLTYPE SetPriority( 
		/* [in] */ LONG nPriority);

	HRESULT STDMETHODCALLTYPE GetPriority( 
		/* [out] */ LONG *pnPriority);

	static void SetBrowser(IDispatch *disp);

private:
	inline boolean InWorkerThread(void) { return WorkThreadId == GetCurrentThreadId(); }
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
	void ProcessConnection(void);
	void InsertRequestHeaders(request_rec *r);
	void InsertBindingHeader(request_rec *r, const char *key, ULONG ulStringType);
	void InsertAdditionalHeaders(request_rec *r, LPOLESTR szHeaders);
	void ReportResponse(request_rec *r);
	void ReportProgress(DWORD statusCode, const char *text);
	void ReportData(apr_bucket_brigade *b);
	void ReportResult(HRESULT result, DWORD errorCode);
	void ProcessCommandHeaders(request_rec *r, const char *type);
	const char *CreateMenuItem(request_rec *r, const char *arg, wa_menu_item_config **pItem);

	// functions used by wa_core
	bool BindApacheRequest(request_rec *r);
	apr_bucket *CreateInputBucket(apr_bucket_alloc_t *list);
	bool ReportApacheResponseStart(request_rec *r);
	bool ReportApacheResponseEnd(request_rec *r);
	bool TakeOutputBuckets(apr_bucket_brigade *b);
	bool RestoreEnvironment(request_rec *r);
	bool SaveEnvironment(request_rec *r);

	// these are really static member functions, written as
	// C functions so they blend in better with Apache code
	friend static int wa_core_process_connection(conn_rec *c);
	friend int wa_core_input_filter(ap_filter_t *f, apr_bucket_brigade *b,
											ap_input_mode_t mode, apr_read_type_e block,
											apr_off_t readbytes);
	friend static apr_status_t wa_http_header_filter(ap_filter_t *f, apr_bucket_brigade *b);
	friend static apr_status_t wa_core_output_filter(ap_filter_t *f, apr_bucket_brigade *b);
	friend static int wa_process_request(request_rec *r);

private:
	ULONG RefCount;
	IUnknown *OuterUnknown;
	apr_pool_t *Pool;
	conn_rec *Connection;
	DWORD WorkThreadId;

	UrlComponents CurrentUrl;
	int HttpMethod;
	char *ContentType;
	const char *Charset;
	char *AttachmentFilename;
	SYSTEMTIME LastModifiedTime;
	IDispatch *Browser;

	bool Terminated;
	bool Redirecting;
	bool NeedFile;
	int HttpStatusCode;

	char *TempFilename;
	HANDLE TempFile;

	WapacheDataDispatcher *DataDispatcher;
	WapacheCommandDispatcher *CommandDispatcher;

	ULONG DataRead;
	ULONG TotalDataLen;

    apr_bucket_brigade *BB;
	const char *AvailData;
	apr_size_t AvailDataLen;

	IInternetProtocolSink *Sink;
	IInternetBindInfo *BindInfo;
	IHttpNegotiate *HttpNegotiate;

	static HANDLE SetBrowserMutex;
	static IDispatch *ActiveBrowser;
};

class WapacheProtocolFactory :
	public IClassFactory 
{
public:
	/***** IUnknown methods *****/
    HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
    ULONG STDMETHODCALLTYPE AddRef(void);
        
    ULONG STDMETHODCALLTYPE Release(void);

	/***** IClassFactory methods *****/
    HRESULT STDMETHODCALLTYPE CreateInstance( 
        /* [unique][in] */ IUnknown __RPC_FAR *pUnkOuter,
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
    
    HRESULT STDMETHODCALLTYPE LockServer( 
        /* [in] */ BOOL fLock);
};

EXTERN_C const GUID CLSID_WapacheProtocol;

extern WapacheProtocolFactory ProtocolFactoryInstance;

#define ProtocolFactory	static_cast<IClassFactory *>(&ProtocolFactoryInstance)

enum { 
	REPORT_PROGRESS,
	REPORT_DATA,
	REPORT_RESULT,
	REPORT_RESPONSE,
	INSERT_HEADERS
};

#endif