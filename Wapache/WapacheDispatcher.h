#ifndef __WAPACHEDATADISPATCHER_H
#define __WAPACHEDATADISPATCHER_H

struct WapacheExecContext;

class WapacheDataDispatcher {
public:
	WapacheDataDispatcher(ap_regex_t *pattern, const char *method, const char *domain, const char *charSet, IDispatch *browser);
	~WapacheDataDispatcher(void);

	bool Process(apr_bucket_brigade *bb);
	void Dispatch(void);

	static void Callback(void *self);

private:
	char *Buffer;
	bool Aborted;
	unsigned long Index;
	unsigned long BufferSize;
	LPOLESTR Method;
	HANDLE CompletionEvent;
	WapacheExecContext *Context;
};

class WapacheCommandDispatcher {
public:
	WapacheCommandDispatcher(const char *domain, const char *charSet, IDispatch *browser);
	~WapacheCommandDispatcher(void);

	void Process(wa_menu_item_config *item);
	void Dispatch(void);

private:
	static void Callback(void *self);

private:
	WapacheExecContext *Context;
	HANDLE CompletionEvent;
	wa_menu_item_config *MenuItem;
};

#endif