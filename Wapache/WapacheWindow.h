#ifndef __WAPACHEWINDOW_H
#define __WAPACHEWINDOW_H

#define IS_MODAL_DLG						0x00000001
#define IS_TOOL_WIN							0x00000002
#define IS_OWNED_WIN						0x00000004
#define DIMENSION_DEFINED					0x00000008
#define MAINTAIN_FRAME_ASPECT_RATIO			0x00000010
#define MAINTAIN_CLIENT_AREA_ASPECT_RATIO	0x00000020

#define TO_BE_MAXIMIZED						0x80000000
#define UNNAMED								0x40000000

#define MAINTAIN_ASPECT_RATIO				(MAINTAIN_FRAME_ASPECT_RATIO | MAINTAIN_CLIENT_AREA_ASPECT_RATIO)

struct WapacheExecContext;

class WapacheExternal;

class WapacheWindow : 
	public IDispatch,
	public IServiceProvider,
	public IOleClientSite, 
	public IOleInPlaceSite,
	public IOleInPlaceFrame,
	public IDocHostUIHandler2,
	public IInternetSecurityManager
{
public:
	WapacheWindow(const char *name, WapacheWindow *parent);
	~WapacheWindow(void);

	HWND Handle(void)	{ return Hwnd; }

	bool Start(void);
	void Close(bool mustClose);
	void Focus(void);
	void StartMoving(void);

	bool CreateScriptObject(VARIANT *p);

	static int ProcessKeyboardEvent(MSG *msg);
	static bool FindWindow(const char *name, WapacheWindow **pWin);
	static bool FindWindowWithFrame(const char *target, WapacheWindow **pWin);
	static bool OpenUrl(const char *openerName, const char *url, const char *target, WapacheWindow **pWin);

	static void InitExecContext(WapacheExecContext *context, IHTMLDocument2 *origindoc);
	static void InitExecContext(WapacheExecContext *context, const char *domain, const char *charSet);
	static void ClearExecContext(WapacheExecContext *context);

	static void SetMenuItemAvailability(WapacheExecContext *context, HMENU hMenu);
	static void ExecMenuCommand(WapacheExecContext *context, HMENU hMenu, UINT idm);
	static void ExecMenuCommand(WapacheExecContext *context, wa_menu_item_config *item);

	static void ExecJSMethod(WapacheExecContext *context, const char *method, VARIANT *arg, int argCount, VARIANT **pResults, int *pResultCount);

	static bool BroadcastData(WapacheExecContext *context, LPOLESTR method, LPOLESTR data);

	static void OnApplicationStart(void);
	static void OnApplicationEnd(void);
	static void OnApacheConfigChange(void);
	static void OnDocumentRootChange(const char *domain);

	static WapacheWindow *FirstWindow(void)	{ return WindowList; }
	static void CloseAllWindows(WapacheWindow *win);

	/***** IUnknown methods *****/
    HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
    ULONG STDMETHODCALLTYPE AddRef(void);
        
    ULONG STDMETHODCALLTYPE Release(void);

	/***** IDispatch methods *****/
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount( 
        /* [out] */ UINT __RPC_FAR *pctinfo);
    
    HRESULT STDMETHODCALLTYPE GetTypeInfo( 
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    
    HRESULT STDMETHODCALLTYPE GetIDsOfNames( 
        /* [in] */ REFIID riid,
        /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
    
    HRESULT STDMETHODCALLTYPE Invoke( 
        /* [in] */ DISPID dispIdMember,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
        /* [out] */ VARIANT __RPC_FAR *pVarResult,
        /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
        /* [out] */ UINT __RPC_FAR *puArgErr);

	/***** IServiceProvider methods *****/
    HRESULT STDMETHODCALLTYPE QueryService( 
        /* [in] */ REFGUID guidService,
        /* [in] */ REFIID riid,
        /* [out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

	/***** IOleClientSite methods *****/
    HRESULT STDMETHODCALLTYPE SaveObject(void);
    
    HRESULT STDMETHODCALLTYPE GetMoniker( 
        /* [in] */ DWORD dwAssign,
        /* [in] */ DWORD dwWhichMoniker,
        /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmk);
    
    HRESULT STDMETHODCALLTYPE GetContainer( 
        /* [out] */ IOleContainer __RPC_FAR *__RPC_FAR *ppContainer);
    
    HRESULT STDMETHODCALLTYPE ShowObject(void);
    
    HRESULT STDMETHODCALLTYPE OnShowWindow( 
        /* [in] */ BOOL fShow);
    
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void);

	/***** IOleInPlaceSite methods *****/
    HRESULT STDMETHODCALLTYPE GetWindow( 
        /* [out] */ HWND __RPC_FAR *phwnd);
    
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp( 
        /* [in] */ BOOL fEnterMode);

    HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void);
    
    HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void);
    
    HRESULT STDMETHODCALLTYPE OnUIActivate(void);
    
    HRESULT STDMETHODCALLTYPE GetWindowContext( 
        /* [out] */ IOleInPlaceFrame __RPC_FAR *__RPC_FAR *ppFrame,
        /* [out] */ IOleInPlaceUIWindow __RPC_FAR *__RPC_FAR *ppDoc,
        /* [out] */ LPRECT lprcPosRect,
        /* [out] */ LPRECT lprcClipRect,
        /* [out] */ LPOLEINPLACEFRAMEINFO lpFrameInfo);
    
    HRESULT STDMETHODCALLTYPE Scroll( 
        /* [in] */ SIZE scrollExtant);
    
    HRESULT STDMETHODCALLTYPE OnUIDeactivate( 
        /* [in] */ BOOL fUndoable);
    
    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void);
    
    HRESULT STDMETHODCALLTYPE DiscardUndoState(void);
    
    HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void);
    
    HRESULT STDMETHODCALLTYPE OnPosRectChange( 
        /* [in] */ LPCRECT lprcPosRect);

	/***** IOleInPlaceFrame *****/
    HRESULT STDMETHODCALLTYPE InsertMenus( 
        /* [in] */ HMENU hmenuShared,
        /* [out][in] */ LPOLEMENUGROUPWIDTHS lpMenuWidths);
    
    HRESULT STDMETHODCALLTYPE SetMenu( 
        /* [in] */ HMENU hmenuShared,
        /* [in] */ HOLEMENU holemenu,
        /* [in] */ HWND hwndActiveObject);
    
    HRESULT STDMETHODCALLTYPE RemoveMenus( 
        /* [in] */ HMENU hmenuShared);
    
    HRESULT STDMETHODCALLTYPE SetStatusText( 
        /* [unique][in] */ LPCOLESTR pszStatusText);
      
    HRESULT STDMETHODCALLTYPE TranslateAccelerator( 
        /* [in] */ LPMSG lpmsg,
        /* [in] */ WORD wID);

    HRESULT STDMETHODCALLTYPE GetBorder( 
        /* [out] */ LPRECT lprectBorder);
    
    HRESULT STDMETHODCALLTYPE RequestBorderSpace( 
        /* [unique][in] */ LPCBORDERWIDTHS pborderwidths);
    
    HRESULT STDMETHODCALLTYPE SetBorderSpace( 
        /* [unique][in] */ LPCBORDERWIDTHS pborderwidths);
    
    HRESULT STDMETHODCALLTYPE SetActiveObject( 
        /* [unique][in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
        /* [unique][string][in] */ LPCOLESTR pszObjName);

	/***** IDocHostUIHandler2 methods *****/
    HRESULT STDMETHODCALLTYPE ShowContextMenu( 
        /* [in] */ DWORD dwID,
        /* [in] */ POINT *ppt,
        /* [in] */ IUnknown *pcmdtReserved,
        /* [in] */ IDispatch *pdispReserved);
    
    HRESULT STDMETHODCALLTYPE GetHostInfo( 
        /* [out][in] */ DOCHOSTUIINFO *pInfo);
    
    HRESULT STDMETHODCALLTYPE ShowUI( 
        /* [in] */ DWORD dwID,
        /* [in] */ IOleInPlaceActiveObject *pActiveObject,
        /* [in] */ IOleCommandTarget *pCommandTarget,
        /* [in] */ IOleInPlaceFrame *pFrame,
        /* [in] */ IOleInPlaceUIWindow *pDoc);
    
    HRESULT STDMETHODCALLTYPE HideUI( void);
    
    HRESULT STDMETHODCALLTYPE UpdateUI( void);
    
    HRESULT STDMETHODCALLTYPE EnableModeless( 
        /* [in] */ BOOL fEnable);
    
    HRESULT STDMETHODCALLTYPE OnDocWindowActivate( 
        /* [in] */ BOOL fActivate);
    
    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate( 
        /* [in] */ BOOL fActivate);
    
    HRESULT STDMETHODCALLTYPE ResizeBorder( 
        /* [in] */ LPCRECT prcBorder,
        /* [in] */ IOleInPlaceUIWindow *pUIWindow,
        /* [in] */ BOOL fRameWindow);
    
    HRESULT STDMETHODCALLTYPE TranslateAccelerator( 
        /* [in] */ LPMSG lpMsg,
        /* [in] */ const GUID *pguidCmdGroup,
        /* [in] */ DWORD nCmdID);
    
    HRESULT STDMETHODCALLTYPE GetOptionKeyPath( 
        /* [out] */ LPOLESTR *pchKey,
        /* [in] */ DWORD dw);
    
    HRESULT STDMETHODCALLTYPE GetDropTarget( 
        /* [in] */ IDropTarget *pDropTarget,
        /* [out] */ IDropTarget **ppDropTarget);
    
    HRESULT STDMETHODCALLTYPE GetExternal( 
        /* [out] */ IDispatch **ppDispatch);
    
    HRESULT STDMETHODCALLTYPE TranslateUrl( 
        /* [in] */ DWORD dwTranslate,
        /* [in] */ OLECHAR *pchURLIn,
        /* [out] */ OLECHAR **ppchURLOut);
    
    HRESULT STDMETHODCALLTYPE FilterDataObject( 
        /* [in] */ IDataObject *pDO,
        /* [out] */ IDataObject **ppDORet);

    HRESULT STDMETHODCALLTYPE GetOverrideKeyPath( 
        /* [out] */ LPOLESTR *pchKey,
        /* [in] */ DWORD dw);

	/***** IInternetSecurityManager *****/
    HRESULT STDMETHODCALLTYPE SetSecuritySite( 
        /* [unique][in] */ IInternetSecurityMgrSite *pSite);
    
    HRESULT STDMETHODCALLTYPE GetSecuritySite( 
        /* [out] */ IInternetSecurityMgrSite **ppSite);
    
    HRESULT STDMETHODCALLTYPE MapUrlToZone( 
        /* [in] */ LPCWSTR pwszUrl,
        /* [out] */ DWORD *pdwZone,
        /* [in] */ DWORD dwFlags);
    
    HRESULT STDMETHODCALLTYPE GetSecurityId( 
        /* [in] */ LPCWSTR pwszUrl,
        /* [size_is][out] */ BYTE *pbSecurityId,
        /* [out][in] */ DWORD *pcbSecurityId,
        /* [in] */ DWORD_PTR dwReserved);
    
    HRESULT STDMETHODCALLTYPE ProcessUrlAction( 
        /* [in] */ LPCWSTR pwszUrl,
        /* [in] */ DWORD dwAction,
        /* [size_is][out] */ BYTE *pPolicy,
        /* [in] */ DWORD cbPolicy,
        /* [in] */ BYTE *pContext,
        /* [in] */ DWORD cbContext,
        /* [in] */ DWORD dwFlags,
        /* [in] */ DWORD dwReserved);
    
    HRESULT STDMETHODCALLTYPE QueryCustomPolicy( 
        /* [in] */ LPCWSTR pwszUrl,
        /* [in] */ REFGUID guidKey,
        /* [size_is][size_is][out] */ BYTE **ppPolicy,
        /* [out] */ DWORD *pcbPolicy,
        /* [in] */ BYTE *pContext,
        /* [in] */ DWORD cbContext,
        /* [in] */ DWORD dwReserved);
    
    HRESULT STDMETHODCALLTYPE SetZoneMapping( 
        /* [in] */ DWORD dwZone,
        /* [in] */ LPCWSTR lpszPattern,
        /* [in] */ DWORD dwFlags);
    
    HRESULT STDMETHODCALLTYPE GetZoneMappings( 
        /* [in] */ DWORD dwZone,
        /* [out] */ IEnumString **ppenumString,
        /* [in] */ DWORD dwFlags);
        
private:
	void ObtainWindowSettings(void);
	void ApplyNewSettings(void);
	bool SetWindowIcon(void);
	static bool HasFrame(IWebBrowser2 *browser, BSTR target);
	static void CloseNextWindow(void);

	void EnableWindow(BOOL enabled);
	void ActivateWindow(UINT state);

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static void ExecCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecStdCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecStdEnvCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecJSCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecJSEnvCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecUrlCommand (WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecUrlEnvCommand (WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecDOMCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void ExecDOMEnvCommand(WapacheExecContext *context, wa_menu_item_config *item);
	static void Broadcast(WapacheExecContext *context);
	static bool CheckExecContext(WapacheExecContext *context);
	static void Dispatch(WapacheExecContext *context, IWebBrowser2 *browser);

	/***** regular Windows event handlers *****/
	void OnCreate(CREATESTRUCT *c);
	bool OnClose(bool mustClose);
	void OnDestroy(void);
	void OnSize(WORD width, WORD height);
	void OnSizing(WORD type, RECT *rect);
	void OnCommand(bool fromAccel, UINT cmdId, HWND control);
	void OnInitMenuPopup(HMENU hMenu, bool isWindowMenu, UINT pos);
	bool OnSetFocus(void);
	bool OnNCActivate(UINT state, HWND otherHwnd, LRESULT *result);
	void OnParentNotify(WPARAM wParam, LPARAM lParam);
	bool OnMoving(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *result);

	/***** ActiveX event handlers *****/

	void OnBeforeNavigate(          
		IDispatch *pDisp,
		VARIANT *&url,
		VARIANT *&Flags,
		VARIANT *&TargetFrameName,
		VARIANT *&PostData,
		VARIANT *&Headers,
		VARIANT_BOOL *&Cancel);

	void OnDocumentComplete(          
		IDispatch *pDisp,
		VARIANT *URL);

	void OnNavigateComplete(          
		IDispatch *pDisp,
		VARIANT *URL);

	void OnNavigateError(
		IDispatch *pDisp,
		VARIANT *URL,
		VARIANT *TargetFrameName,
		VARIANT *StatusCode,
		VARIANT_BOOL *&Cancel);

	void OnNewWindow(          
		IDispatch **&ppDisp,
		BSTR Url,
		DWORD Flags,
		BSTR TargetFrameName,
		VARIANT *&PostData,
		BSTR Headers,
		VARIANT_BOOL *&Cancel);

	void OnTitleChange(          
		BSTR title);

	void OnStatusTextChange(
		BSTR title);

	bool ExecOnCloseHandler(WapacheExecContext *context);

private:
	WapacheWindow *Parent;
	WapacheWindow *Prev;
	WapacheWindow *Next;

	ULONG RefCount;
	DWORD Flags;
	DWORD HostUIFlags;
	CREATESTRUCT CreateStruct;
	char IconPath[MAX_PATH + 1];

	HWND Hwnd;
	char *Name;
	IOleObject *Control;
	IOleInPlaceActiveObject *InPlaceObject;
	IWebBrowser2 *Browser;
	IDispatch *External;
	RECT ClientRect;
	DWORD AdviseConn;
	DWORD AdviseConn2;

	POINT CornerRadii; 
	bool Moving;
	POINT OriginalPos;
	POINT DragPoint;
	double AspectRatio;
	SIZE FrameOffset;
	SIZE MinimumSize;

	static ATOM ClassAtom;
	static WapacheWindow *WindowList;
	static WapacheWindow *LastWindowToClose;

private:
	IDispatch **you_dont_see_me;

	friend WapacheExternal;
};

typedef void (*WapacheExecFunc)(WapacheExecContext *);

#define TEST_DONT_EXEC			0x80000000

#define DONT_CHECK_SCHEME		0x00000001
#define DONT_CHECK_DOMAIN		0x00000002

#define ITEM_AVAILABLE			0x00000001
#define ENV_SET					0x00000002

struct WapacheExecContext {
	DWORD Flags;
	apr_pool_t *TemporaryPool;
	const char *Name;
	ap_regex_t *Pattern;
	const char *Scheme;
	const char *Domain;
	DWORD Codepage;
	IHTMLWindow2 *OriginWin;

	WapacheExecFunc Function;
	void *Data;

	IWebBrowser2 *Browser;
	IHTMLWindow2 *Win;
	IHTMLDocument2 *Doc;
	WapacheWindow *WinObj;

	DWORD Result;
	bool Aborted;
};

#endif
