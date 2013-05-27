#include "stdafx.h"
#include "WapacheDispatcher.h"
#include "WapacheApplication.h"
#include "WapacheWindow.h"

IHTMLWindow2 *GetOriginWin(IDispatch *browser) {
	HRESULT hr;
	IWebBrowser2 *wb;
	IDispatch *docDisp;
	IHTMLDocument2 *doc = NULL;
	IHTMLWindow2 *win = NULL;
	if(browser) {
		hr = browser->QueryInterface(&wb);
		if(hr == S_OK) {
			hr = wb->get_Document(&docDisp);
			wb->Release();
		}
		if(hr == S_OK) {
			hr = docDisp->QueryInterface(&doc);
			docDisp->Release();
		}
		if(hr == S_OK) {
			hr = doc->get_parentWindow(&win);
			doc->Release();
		}
	}
	return win;
}

WapacheDataDispatcher::WapacheDataDispatcher(regex_t *pattern, const char *method, const char *domain, const char *charSet, IDispatch *browser)
{
	Buffer = NULL;
	Index = 0;
	BufferSize = 0;
	Aborted = false;
	Context = new WapacheExecContext;
	Method = CStrToWideString(method, -1);
	WapacheWindow::InitExecContext(Context, domain, charSet);
	CompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Context->Pattern = pattern;
	Context->OriginWin = GetOriginWin(browser);
}

WapacheDataDispatcher::~WapacheDataDispatcher(void)
{
	delete[] Buffer;
	SysFreeString(Method);
	CloseHandle(CompletionEvent);
	WapacheWindow::ClearExecContext(Context);
	delete Context;
}

bool WapacheDataDispatcher::Process(apr_bucket_brigade *bb)
{
	if(Aborted) {
		return false;
	}
	while(!APR_BRIGADE_EMPTY(bb)) {
		apr_bucket *first = APR_BRIGADE_FIRST(bb);
		apr_size_t len;
		const char *data;
		apr_status_t rv;
		rv = apr_bucket_read(first, &data, &len, APR_BLOCK_READ);
		if(rv == APR_SUCCESS) {
			for(int i = 0; i < len; i++) {
				if(Index >= BufferSize) {
					BufferSize += 1024;
					char *newBuf = new char[BufferSize];
					memcpy(newBuf, Buffer, Index);
					delete[] Buffer;
					Buffer = newBuf;
				}
				Buffer[Index] = data[i];
				if(Buffer[Index] == '\n') {
					// switch to the main thread to dispatch the line
					Application.Switch(Callback, this);
					WaitForSingleObject(CompletionEvent, INFINITE);
					Index = 0;
				}
				else {
					Index++;
				}
			}
		}
		if(APR_BUCKET_IS_EOS(first)) {
			// in case the last line doesn't end with a linefeed
			if(Index > 0) {
				Application.Switch(Callback, this);
				WaitForSingleObject(CompletionEvent, INFINITE);
			}
			break;
		}
		apr_bucket_delete(first);
	}
	return true;
}

void WapacheDataDispatcher::Callback(void *data)
{
	WapacheDataDispatcher *self = reinterpret_cast<WapacheDataDispatcher *>(data);
	self->Dispatch();
}

void WapacheDataDispatcher::Dispatch(void) {
	// don't include trailing linefeed
	int len = Index;
	while(len > 0 && (Buffer[len - 1] == '\r' || Buffer[len - 1] == '\n')) len--;

	// convert to Unicode
	LPOLESTR data = Application.ConvertString(Context->Codepage, Buffer, len);

	if(!WapacheWindow::BroadcastData(Context, Method, data)) {
		Aborted = true;
	}

	SetEvent(CompletionEvent);
}

WapacheCommandDispatcher::WapacheCommandDispatcher(const char *domain, const char *charSet, IDispatch *browser)
{
	MenuItem = NULL;
	Context = new WapacheExecContext;
	WapacheWindow::InitExecContext(Context, domain, charSet);
	CompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Context->OriginWin = GetOriginWin(browser);
}

WapacheCommandDispatcher::~WapacheCommandDispatcher(void)
{
	WapacheWindow::ClearExecContext(Context);
	CloseHandle(CompletionEvent);
	delete Context;
}

void WapacheCommandDispatcher::Process(wa_menu_item_config *item)
{
	MenuItem = item;
	Application.Switch(Callback, this);
	WaitForSingleObject(CompletionEvent, INFINITE);
}

void WapacheCommandDispatcher::Dispatch(void)
{
	WapacheWindow::ExecMenuCommand(Context, MenuItem);
	SetEvent(CompletionEvent);
}

void WapacheCommandDispatcher::Callback(void *data)
{
	WapacheCommandDispatcher *self = reinterpret_cast<WapacheCommandDispatcher *>(data);	
	self->Dispatch();
}
