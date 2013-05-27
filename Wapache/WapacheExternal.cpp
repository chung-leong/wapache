#include "stdafx.h"
#include "WapacheExternal.h"
#include "WapacheApplication.h"
#include "WapacheWindow.h"

WapacheExternal::WapacheExternal(WapacheWindow *win)
{
	RefCount = 0;
	Window = win;
	Window->AddRef();
}

WapacheExternal::~WapacheExternal(void)
{
	Window->Release();
}

HRESULT STDMETHODCALLTYPE WapacheExternal::QueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if(riid == IID_IUnknown) {
		*ppvObject = static_cast<IUnknown *>(this);
	}
	else if(riid == IID_IDispatch) {
		*ppvObject = static_cast<IDispatch *>(this);
	}
	else {
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE WapacheExternal::AddRef(void)
{
	return ++RefCount;
}
    
ULONG STDMETHODCALLTYPE WapacheExternal::Release(void)
{
	if(RefCount-- == 1) {
		delete this;
		return 0;
	}
	return RefCount;
}

HRESULT STDMETHODCALLTYPE WapacheExternal::GetTypeInfoCount( 
    /* [out] */ UINT __RPC_FAR *pctinfo)
{
	*pctinfo = 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheExternal::GetTypeInfo( 
    /* [in] */ UINT iTInfo,
    /* [in] */ LCID lcid,
    /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
{
	return E_NOTIMPL;
}

enum {
	DISPID_MESSAGEBOX = 1,
	DISPID_STARTMOVINGWINDOW,
	DISPID_SHOWWINDOW,
	DISPID_ANIMATEWINDOW,
	DISPID_CLOSEWINDOW,
	DISPID_EXITPROGRAM,
	DISPID_BROWSEFORFOLDER,
	DISPID_GETOPENFILENAME,
	DISPID_GETSAVEFILENAME,
	DISPID_INTERNETGETCONNECTEDSTATE,
	DISPID_INTERNETAUTODIAL,
	DISPID_INTERNETAUTODIALHANGUP,
	DISPID_INTERNETGOONLINE,
	DISPID_SHELLEXECUTE,
	DISPID_SESSION,
};

HRESULT STDMETHODCALLTYPE WapacheExternal::GetIDsOfNames( 
    /* [in] */ REFIID riid,
    /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
    /* [in] */ UINT cNames,
    /* [in] */ LCID lcid,
    /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
{
	if(wcsicmp(rgszNames[0], L"MessageBox") == 0) {
		*rgDispId = DISPID_MESSAGEBOX;
	}
	else if(wcsicmp(rgszNames[0], L"StartMovingWindow") == 0) {
		*rgDispId = DISPID_STARTMOVINGWINDOW;
	}
	else if(wcsicmp(rgszNames[0], L"ShowWindow") == 0) {
		*rgDispId = DISPID_SHOWWINDOW;
	}
	else if(wcsicmp(rgszNames[0], L"AnimateWindow") == 0) {
		*rgDispId = DISPID_ANIMATEWINDOW;
	}
	else if(wcsicmp(rgszNames[0], L"CloseWindow") == 0) {
		*rgDispId = DISPID_CLOSEWINDOW;
	}
	else if(wcsicmp(rgszNames[0], L"ExitProgram") == 0) {
		*rgDispId = DISPID_EXITPROGRAM;
	}
	else if(wcsicmp(rgszNames[0], L"BrowseForFolder") == 0) {
		*rgDispId = DISPID_BROWSEFORFOLDER;
	}
	else if(wcsicmp(rgszNames[0], L"GetOpenFileName") == 0) {
		*rgDispId = DISPID_GETOPENFILENAME;
	}
	else if(wcsicmp(rgszNames[0], L"GetSaveFileName") == 0) {
		*rgDispId = DISPID_GETSAVEFILENAME;
	}
	else if(wcsicmp(rgszNames[0], L"InternetGetConnectedState") == 0) {
		*rgDispId = DISPID_INTERNETGETCONNECTEDSTATE;
	}
	else if(wcsicmp(rgszNames[0], L"InternetAutodial") == 0) {
		*rgDispId = DISPID_INTERNETAUTODIAL;
	}
	else if(wcsicmp(rgszNames[0], L"InternetAutodialHangup") == 0) {
		*rgDispId = DISPID_INTERNETAUTODIALHANGUP;
	}
	else if(wcsicmp(rgszNames[0], L"InternetGoOnline") == 0) {
		*rgDispId = DISPID_INTERNETGOONLINE;
	}
	else if(wcsicmp(rgszNames[0], L"ShellExecute") == 0) {
		*rgDispId = DISPID_SHELLEXECUTE;
	}
	else if(wcsicmp(rgszNames[0], L"session") == 0) {
		*rgDispId = DISPID_SESSION;
	}
	else {
		return DISP_E_UNKNOWNNAME;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheExternal::Invoke( 
    /* [in] */ DISPID dispIdMember,
    /* [in] */ REFIID riid,
    /* [in] */ LCID lcid,
    /* [in] */ WORD wFlags,
    /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
    /* [out] */ VARIANT __RPC_FAR *pVarResult,
    /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
    /* [out] */ UINT __RPC_FAR *puArgErr)
{
	HRESULT hr = DISP_E_MEMBERNOTFOUND;
	if(wFlags & DISPATCH_METHOD) {
		switch(dispIdMember) {
			case DISPID_MESSAGEBOX:
				hr = MessageBox(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_STARTMOVINGWINDOW:
				hr = StartMovingWindow(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_SHOWWINDOW:
				hr = ShowWindow(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_ANIMATEWINDOW:
				hr = AnimateWindow(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_CLOSEWINDOW:
				hr = CloseWindow(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_EXITPROGRAM:
				hr = ExitProgram(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_BROWSEFORFOLDER:
				hr = BrowseForFolder(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_GETOPENFILENAME:
				hr = GetOpenFileName(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_GETSAVEFILENAME:
				hr = GetSaveFileName(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_INTERNETGETCONNECTEDSTATE:
				hr = InternetGetConnectedState(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_INTERNETAUTODIAL:
				hr = InternetAutodialHangup(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_INTERNETAUTODIALHANGUP:
				hr = InternetAutodialHangup(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_INTERNETGOONLINE:
				hr = InternetGoOnline(pDispParams, pVarResult, puArgErr);
				break;
			case DISPID_SHELLEXECUTE:
				hr = ShellExecute(pDispParams, pVarResult, puArgErr);
				break;
		}
	} else if(wFlags & DISPATCH_PROPERTYGET) {
		switch(dispIdMember) {
			case DISPID_SESSION:
				if(V_VT(&Application.ScriptSessionObject) == VT_EMPTY) {
					Window->CreateScriptObject(&Application.ScriptSessionObject);
				}
				VariantCopy(pVarResult, &Application.ScriptSessionObject);
				hr = S_OK;
				break;
		}
	}
	return hr;
}

HRESULT WapacheExternal::MessageBox(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 3) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT msgV, captionV, flagsV;
	VariantInit(&msgV);
	VariantInit(&captionV);
	VariantInit(&flagsV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&msgV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&captionV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&flagsV, &argv[--i], 0, VT_I4))) {
		int flags = V_I4(&flagsV);
		int result = ::MessageBoxW(Window->Hwnd, V_BSTR(&msgV), V_BSTR(&captionV), flags);
		if(pVarResult) {
			V_VT(pVarResult) = VT_I4;
			V_I4(pVarResult) = result;
		}
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	VariantClear(&msgV);
	VariantClear(&captionV);
	VariantClear(&flagsV);
	return hr;
}

HRESULT WapacheExternal::StartMovingWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	Window->StartMoving();
	return S_OK;
}

HRESULT WapacheExternal::ShowWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 1) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT cmdV;
	VariantInit(&cmdV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&cmdV, &argv[--i], 0, VT_I4))) {
		int cmd = V_I4(&cmdV);
		BOOL result = ::ShowWindow(Window->Hwnd, cmd);
		if(pVarResult) {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = result;
		}
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	VariantClear(&cmdV);
	return hr;
}

HRESULT WapacheExternal::AnimateWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 2) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT timeV, flagsV;
	VariantInit(&timeV);
	VariantInit(&flagsV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&timeV, &argv[--i], 0, VT_I4))
	&& SUCCEEDED(VariantChangeType(&flagsV, &argv[--i], 0, VT_I4))) {
		DWORD time = V_I4(&timeV);
		DWORD flags = V_I4(&flagsV);
		BOOL result = ::AnimateWindow(Window->Hwnd, time, flags);
		if(pVarResult) {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = result;
		}
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	VariantClear(&timeV);
	VariantClear(&flagsV);
	return hr;
}

HRESULT WapacheExternal::CloseWindow(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	Window->Close(false);
	return S_OK;
}

HRESULT WapacheExternal::ExitProgram(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	// close all the windows
	WapacheWindow::CloseAllWindows(Window);
	return S_OK;
}

HRESULT WapacheExternal::BrowseForFolder(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 2) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT titleV, flagsV;
	VariantInit(&titleV);
	VariantInit(&flagsV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&titleV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&flagsV, &argv[--i], 0, VT_I4))) {
		WCHAR buffer[MAX_PATH];
		BROWSEINFOW bi;
		ZeroMemory(&bi, sizeof(bi));
		bi.hwndOwner = Window->Hwnd;
		if(V_VT(&argv[1]) != VT_BOOL) {
			bi.lpszTitle = V_BSTR(&titleV);
		}
		bi.ulFlags = V_I4(&flagsV);
		bi.pszDisplayName = buffer;

		V_VT(pVarResult) = VT_NULL;

		LPITEMIDLIST pidl = ::SHBrowseForFolderW(&bi);
		if(pidl) {
			if(SHGetPathFromIDListW(pidl, buffer)) {
				V_VT(pVarResult) = VT_BSTR;
				V_BSTR(pVarResult) = SysAllocString(buffer);
			}
		}
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	VariantClear(&titleV);
	VariantClear(&flagsV);
	return hr;
}

HRESULT WapacheExternal::GetOpenFileName(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 3) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT titleV, filePathV, filterV;
	VariantInit(&titleV);
	VariantInit(&filePathV);
	VariantInit(&filterV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&titleV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&filePathV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&filterV, &argv[--i], 0, VT_BSTR))) {
		WCHAR originalPath[MAX_PATH + 1];
		WCHAR fileBuffer[MAX_PATH + 1];
		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = Window->Hwnd;
		ofn.hInstance = _hinstance;
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
		if(V_VT(&argv[2]) != VT_BOOL) {
			ofn.lpstrTitle = V_BSTR(&titleV);
		}
		ZeroMemory(fileBuffer, sizeof(fileBuffer));
		if(V_VT(&argv[1]) != VT_BOOL) {
			wcsncpy(fileBuffer, V_BSTR(&filePathV), MAX_PATH);
		}
		ofn.lpstrFile = fileBuffer;
		ofn.nMaxFile = MAX_PATH + 1;
		if(V_VT(&argv[0]) != VT_BOOL) {
			BSTR filter = V_BSTR(&filterV);
			int len = (int) wcslen(filter);
			WCHAR *filterList = new WCHAR[len + 2];
			for(int i = 0; filter[i] != '\0'; i++) {
				if(filter[i] == '|') {
					filterList[i] = '\0';
				}
				else {
					filterList[i] = filter[i];
				}
			}
			filterList[len] = '\0';
			filterList[len + 1] = '\0';
			ofn.lpstrFilter = filterList;
		}
		GetCurrentDirectoryW(sizeof(originalPath), originalPath);
		if(::GetOpenFileNameW(&ofn)) {
			V_VT(pVarResult) = VT_BSTR;
			V_BSTR(pVarResult) = SysAllocString(fileBuffer);
		}
		SetCurrentDirectoryW(originalPath);
		delete[] (WCHAR *) ofn.lpstrFilter;
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	return hr;
}

HRESULT WapacheExternal::GetSaveFileName(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 3) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT titleV, filePathV, filterV;
	VariantInit(&titleV);
	VariantInit(&filePathV);
	VariantInit(&filterV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&titleV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&filePathV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&filterV, &argv[--i], 0, VT_BSTR))) {
		WCHAR originalPath[MAX_PATH + 1];
		WCHAR fileBuffer[MAX_PATH + 1];
		WCHAR initialFolder[MAX_PATH + 1];
		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = Window->Hwnd;
		ofn.hInstance = _hinstance;
		ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_ENABLESIZING;
		if(V_VT(&argv[2]) != VT_BOOL) {
			ofn.lpstrTitle = V_BSTR(&titleV);
		}
		ZeroMemory(fileBuffer, sizeof(fileBuffer));
		ZeroMemory(initialFolder, sizeof(initialFolder));
		if(V_VT(&argv[1]) != VT_BOOL) {
			WCHAR *path = V_BSTR(&filePathV);
			WCHAR *slash = wcsrchr(path, '\\');
			if(slash) {
				*slash = '\0';
				wcsncpy(fileBuffer, slash + 1, MAX_PATH);
				wcsncpy(initialFolder, path, MAX_PATH);
			}
			delete[] path;
		}
		ofn.lpstrFile = fileBuffer;
		ofn.lpstrInitialDir = initialFolder;
		ofn.nMaxFile = MAX_PATH + 1;
		if(V_VT(&argv[0]) != VT_BOOL) {
			BSTR filter = V_BSTR(&filterV);
			int len = (int) wcslen(filter);
			WCHAR *filterList = new WCHAR[len + 2];
			for(int i = 0; filter[i] != '\0'; i++) {
				if(filter[i] == '|') {
					filterList[i] = '\0';
				}
				else {
					filterList[i] = filter[i];
				}
			}
			filterList[len] = '\0';
			filterList[len + 1] = '\0';
			ofn.lpstrFilter = filterList;
		}
		GetCurrentDirectoryW(sizeof(originalPath), originalPath);
		if(::GetSaveFileNameW(&ofn)) {
			V_VT(pVarResult) = VT_BSTR;
			V_BSTR(pVarResult) = SysAllocString(fileBuffer);
		}
		SetCurrentDirectoryW(originalPath);

		delete[] (WCHAR *) ofn.lpstrFilter;
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	return hr;
}

#define INTERNET_CONNECTION_AVAILABLE	0x00010000

HRESULT WapacheExternal::InternetGetConnectedState(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;

	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	DWORD flags = 0;
	if(::InternetGetConnectedState(&flags, 0)) {
		flags |= INTERNET_CONNECTION_AVAILABLE;
	}

	V_VT(pVarResult) = VT_I4;
	V_I4(pVarResult) = flags;

	return hr;
}

HRESULT WapacheExternal::InternetAutodial(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;

	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	if(::InternetAutodial(0, Window->Hwnd)) {
		V_VT(pVarResult) = VT_BOOL;
		V_BOOL(pVarResult) = TRUE;
	}
	else {
		V_VT(pVarResult) = VT_BOOL;
		V_BOOL(pVarResult) = FALSE;
	}

	return hr;
}

HRESULT WapacheExternal::InternetAutodialHangup(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;

	if(argc != 0) {
		return DISP_E_BADPARAMCOUNT;
	}

	if(::InternetAutodialHangup(0)) {
		V_VT(pVarResult) = VT_BOOL;
		V_BOOL(pVarResult) = TRUE;
	}
	else {
		V_VT(pVarResult) = VT_BOOL;
		V_BOOL(pVarResult) = FALSE;
	}

	return hr;
}

HRESULT WapacheExternal::InternetGoOnline(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 1) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT urlV;
	VariantInit(&urlV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&urlV, &argv[--i], 0, VT_BSTR))) {
		char *url = WideStringToCStr(V_BSTR(&urlV), -1);
		if(::InternetGoOnline(url, Window->Hwnd, 0)) {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = TRUE;
		}
		else {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = FALSE;
		}
		delete[] url;
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	return hr;
}

HRESULT WapacheExternal::ShellExecute(DISPPARAMS *pDispParams, VARIANT *pVarResult, UINT *puArgErr)
{
	HRESULT hr = S_OK;
	unsigned int argc = pDispParams->cArgs;
	VARIANTARG *argv = pDispParams->rgvarg;
	
	if(argc != 2) {
		return DISP_E_BADPARAMCOUNT;
	}

	VARIANT operationV, fileV;
	VariantInit(&operationV);
	VariantInit(&fileV);

	int i = argc;
	if(SUCCEEDED(VariantChangeType(&operationV, &argv[--i], 0, VT_BSTR))
	&& SUCCEEDED(VariantChangeType(&fileV, &argv[--i], 0, VT_BSTR))) {
		HINSTANCE result = ::ShellExecuteW(Window->Hwnd, (V_VT(&argv[1]) != VT_BOOL) ? V_BSTR(&operationV) : NULL, V_BSTR(&fileV), NULL, NULL, SW_SHOWNORMAL);
		if((int) result > 32) {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = TRUE;
		}
		else {
			V_VT(pVarResult) = VT_BOOL;
			V_BOOL(pVarResult) = FALSE;
		}
	}
	else {
		*puArgErr = i;
		hr = DISP_E_BADVARTYPE;		
	}

	return hr;
}
