#ifndef __WAPACHEEXTERNAL_H
#define __WAPACHEEXTERNAL_H

class WapacheWindow;

class WapacheExternal : public IDispatch {
public:
	WapacheExternal(WapacheWindow *win);
	~WapacheExternal(void);

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

private:
	HRESULT MessageBox(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT StartMovingWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT ShowWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT AnimateWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT CloseWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT ExitProgram(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT BrowseForFolder(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT GetOpenFileName(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT GetSaveFileName(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT InternetGetConnectedState(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT InternetAutodial(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT InternetAutodialHangup(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT InternetGoOnline(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);
	HRESULT ShellExecute(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr);

private:
	ULONG RefCount;
	WapacheWindow *Window;
};

#endif