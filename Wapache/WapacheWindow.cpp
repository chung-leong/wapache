#include "stdafx.h"
#include "WapacheWindow.h"
#include "WapacheApplication.h"
#include "WapacheProtocol.h"
#include "WapacheExternal.h"

ATOM WapacheWindow::ClassAtom = 0;
WapacheWindow *WapacheWindow::WindowList = NULL;
WapacheWindow *WapacheWindow::LastWindowToClose = NULL;

WapacheWindow::WapacheWindow(const char *name, WapacheWindow *parent) 
{
	RefCount = 0;
	Name = strdup(name);
	Hwnd = NULL;
	Parent = parent;
	Control = NULL;
	Browser = NULL;
	Flags = 0;
	InPlaceObject = NULL;
	if(Parent) parent->AddRef();
	External = new WapacheExternal(this);
	External->AddRef();
	Moving = false;
	AspectRatio = 0;
	FrameOffset.cx = FrameOffset.cy = 0;
	MinimumSize.cx = MinimumSize.cy = 0;
	you_dont_see_me = NULL;
}

WapacheWindow::~WapacheWindow(void) 
{
	if(Hwnd) DestroyWindow(Hwnd);
	if(Control) Control->Release();
	if(Browser) Browser->Release();
	if(Parent) Parent->Release();
	External->Release();
	delete[] Name;
}

bool WapacheWindow::Start(void) {

	if(!ClassAtom) {
		WNDCLASSEX wc;

		wc.cbSize = sizeof(wc);
		wc.style = 0;
		wc.lpfnWndProc = WindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hIcon = NULL;
		wc.hIconSm = NULL;
		wc.hInstance = _hinstance;
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = "Wapache";

		ClassAtom = RegisterClassEx(&wc);
	}

	ObtainWindowSettings();
	CreateWindowEx(CreateStruct.dwExStyle, "Wapache", "", CreateStruct.style, CreateStruct.x, CreateStruct.y, CreateStruct.cx, CreateStruct.cy, CreateStruct.hwndParent, CreateStruct.hMenu, CreateStruct.hInstance, reinterpret_cast<LPVOID>(this));
	if(!Hwnd || !Browser) {
		return false;
	}
	return true;
}

void WapacheWindow::ApplyNewSettings(void) {
	CREATESTRUCT oldCreateStruct = CreateStruct;
	DWORD oldFlags = Flags;
	DWORD oldHostUIFlags = HostUIFlags;
	POINT oldCornerRadii = CornerRadii;
	char oldIconPath[MAX_PATH + 1];
	strcpy(oldIconPath, IconPath);
	HWND oldParent = GetParent(Hwnd);

	// can't change from an owned window to an un-owned window or vice-versa
	// close the window
	if(oldParent != CreateStruct.hwndParent) {
		PostMessage(Hwnd, WM_CLOSE, 0, 0);
	}

	// apply new styles and positioning
	DWORD swpFlags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER;
	ObtainWindowSettings();
	if(oldCreateStruct.x != CreateStruct.x || oldCreateStruct.y != CreateStruct.y) {
		swpFlags &= ~SWP_NOMOVE;
	}
	if(oldCreateStruct.cx != CreateStruct.cx || oldCreateStruct.cy != CreateStruct.cy) {
		swpFlags &= ~SWP_NOSIZE;
	}
	if(oldCreateStruct.style != CreateStruct.style) {
		SetWindowLong(Hwnd, GWL_STYLE, CreateStruct.style);
		swpFlags |= SWP_FRAMECHANGED;
	}
	if(oldCreateStruct.dwExStyle != CreateStruct.dwExStyle) {
		SetWindowLong(Hwnd, GWL_EXSTYLE, CreateStruct.dwExStyle);
		swpFlags |= SWP_FRAMECHANGED;
	}
	SetWindowPos(Hwnd, NULL, CreateStruct.x, CreateStruct.y, CreateStruct.cx, CreateStruct.cy, swpFlags);

	// new menu
	::SetMenu(Hwnd, CreateStruct.hMenu);
	Application.DestroyMenu(oldCreateStruct.hMenu);

	// new icon
	if(stricmp(oldIconPath, IconPath) != 0) {
		SetWindowIcon();
	}

	// refresh the browser if necessary
	if(oldHostUIFlags != HostUIFlags) {
		// REFRESH_CONTINUE apparently means refresh without refetching the page
		VARIANT level;
		V_VT(&level) = VT_I4;
		V_I4(&level) = 2;
		Browser->Refresh2(&level);
	}

	// rounded corners
	if(oldCornerRadii.x != CornerRadii.x || oldCornerRadii.y != CornerRadii.y) {
		RECT rect;
		GetWindowRect(Hwnd, &rect);
		InvalidateRect(NULL, &rect, FALSE);
		HRGN rgn;
		if(CornerRadii.x > 0 && CornerRadii.y > 0) {
			rgn = CreateRoundRectRgn(0, 0, CreateStruct.cx + 1, CreateStruct.cy + 1, CornerRadii.x, CornerRadii.y);
		}
		else {
			rgn = NULL;
		}
		SetWindowRgn(Hwnd, rgn, true);
	}
}

void WapacheWindow::Close(bool mustClose) {
	PostMessage(Hwnd, WM_CLOSE, mustClose ? 1 : 0, 0);
}

void WapacheWindow::CloseAllWindows(WapacheWindow *win) {
	if(!LastWindowToClose) {
		LastWindowToClose = win;
		CloseNextWindow();
	}
}

void WapacheWindow::CloseNextWindow(void) {
	WapacheWindow *to_close = NULL;
	if(WindowList) {
		for(WapacheWindow *w = WindowList->Prev; w != WindowList; w = w->Prev) {
			if(w != LastWindowToClose) {
				to_close = w;
				break;
			}
		}
	}
	if(to_close) {
		to_close->Close(false);
	} else if(LastWindowToClose) {
		// all the other window have been closed, time to close the window that called ExitProgram
		LastWindowToClose->Close(true);
		LastWindowToClose = NULL;
	}
}

void WapacheWindow::Focus(void) {
	if(IsIconic(Hwnd)) {
		ShowWindow(Hwnd, SW_SHOWNORMAL);
	}
	else {
		ShowWindow(Hwnd, SW_SHOW);
	}
	//SetActiveWindow(Hwnd);
	SetForegroundWindow(Hwnd);
}

void WapacheWindow::StartMoving(void) {
	RECT rect;
	Moving = true;
	GetCursorPos(&DragPoint);
	GetWindowRect(Hwnd, &rect);
	OriginalPos.x = rect.left;
	OriginalPos.y = rect.top;
	SetCapture(Hwnd);
}

int GetPixelSize(wa_win_dim dim, int length, int dpi) {
	switch(dim.unit) {
		case PIXEL: return dim.value;
		case SCALED_PIXEL: return dim.value * dpi / 96;
		case PERCENT: return length * dim.value / 100;
		case INCH: return dim.value * dpi;
	}
	return 0;
}

#define DEFINED(b)	(conf->b.unit != 0)
#define HPIXEL(b)	GetPixelSize(conf->b, waWidth, dpi)
#define VPIXEL(b)	GetPixelSize(conf->b, waHeight, dpi)
void WapacheWindow::ObtainWindowSettings(void) 
{
	Application.LockApacheConfig(READ_LOCK);
	wa_win_config *conf = Application.GetWindowSettings(Name);

	int d = sizeof(bool);

	ZeroMemory(&CreateStruct, sizeof(CreateStruct));
	Flags = 0;
	HostUIFlags = DOCHOSTUIFLAG_DISABLE_HELP_MENU
				 | DOCHOSTUIFLAG_ENABLE_INPLACE_NAVIGATION
				 | DOCHOSTUIFLAG_IME_ENABLE_RECONVERSION
				 | DOCHOSTUIFLAG_NOPICS;

	if(!Parent) {
		Flags |= UNNAMED;
	}

	// build the dropdown menu if there's one
	if(conf->DropDownMenu) {
		CreateStruct.hMenu = Application.BuildDropdownMenu(conf->DropDownMenu);
	}

	if(conf->Theme || Application.ClientConf->Theme) {
		HostUIFlags |= DOCHOSTUIFLAG_THEME;
	} else {
		HostUIFlags |= DOCHOSTUIFLAG_NOTHEME;
	}

	// determine the window style
	CreateStruct.style = WS_VISIBLE | WS_CAPTION;
	switch(conf->Type) {
		case STANDARD_WINDOW:
			CreateStruct.style |= WS_OVERLAPPED;
			CreateStruct.dwExStyle |= WS_EX_APPWINDOW;
			break;
		case TOOL_WINDOW:
			CreateStruct.style |= WS_POPUP;
			CreateStruct.dwExStyle |= WS_EX_TOOLWINDOW;
			Flags |= IS_TOOL_WIN;
			if(Parent) {
				CreateStruct.hwndParent = Parent->Hwnd;
			}
			break;
		case DIALOG_BOX:
			CreateStruct.style |= WS_POPUP;
			CreateStruct.dwExStyle |= WS_EX_DLGMODALFRAME;
			HostUIFlags |= DOCHOSTUIFLAG_DIALOG;
			if(conf->Modal) {
				Flags |= IS_MODAL_DLG;
			}
			if(Parent) {
				CreateStruct.hwndParent = Parent->Hwnd;
			}
			break;
	}

	if(conf->MaximizeButton) {
		CreateStruct.style |= WS_MAXIMIZEBOX | WS_SYSMENU;
	}
	if(conf->MinimizeButton) {
		CreateStruct.style |= WS_MINIMIZEBOX | WS_SYSMENU;
	}
	if(conf->SystemMenu) {
		CreateStruct.style |= WS_SYSMENU;
	}
	if(conf->Resizeable) {
		CreateStruct.style |= WS_SIZEBOX;
	}
	else {
		CreateStruct.style |= WS_BORDER;
	}
	if(conf->HelpButton) {
		CreateStruct.style |= WS_SYSMENU;
		CreateStruct.dwExStyle |= WS_EX_CONTEXTHELP;
		CreateStruct.style &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX); 
	}
	if(conf->Maximized) {
		CreateStruct.style |= WS_MAXIMIZE;
	}
	if(conf->FullScreen || conf->Frameless) {
		CreateStruct.style &= ~(WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX | WS_CAPTION | WS_BORDER);
		CreateStruct.style |= WS_POPUP;
	}

	if(!conf->InnerBorder) {
		HostUIFlags |= DOCHOSTUIFLAG_NO3DOUTERBORDER;
	}
	if(!conf->ThreeDBorder) {
		HostUIFlags |= DOCHOSTUIFLAG_NO3DBORDER;
	}
	if(conf->AutoComplete) {
		HostUIFlags |= DOCHOSTUIFLAG_ENABLE_FORMS_AUTOCOMPLETE;
	}
	if(!conf->ScrollBar) {
		HostUIFlags |= DOCHOSTUIFLAG_SCROLL_NO;
	}
	if(conf->FlatScrollBar) {
		HostUIFlags |= DOCHOSTUIFLAG_FLAT_SCROLLBAR;
	}

	// get the work area and dpi
	RECT waRect;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &waRect, 0);
	HDC screen = GetDC(0);
	int dpi = GetDeviceCaps(screen, LOGPIXELSX);
	ReleaseDC(0, screen);
	int waWidth = waRect.right - waRect.left;
	int waHeight = waRect.bottom - waRect.top;

	// get the offset rectangle
	RECT ofRect;
	if(conf->Type == STANDARD_WINDOW || Parent == NULL) {
		ofRect = waRect;
	}
	else {
		GetWindowRect(Parent->Hwnd, &ofRect);
	}
	int ofWidth = ofRect.right - ofRect.left;
	int ofHeight = ofRect.bottom - ofRect.top;

	int width = HPIXEL(Width);
	int height = VPIXEL(Height);

	// calculate the window size based on requested client size
	// if the absolute width and/or height is not given
	if(!DEFINED(Width) || !DEFINED(Height)) {
		RECT r;
		r.left = 0;
		r.right = HPIXEL(ClientWidth);
		r.top = 0;
		r.bottom = VPIXEL(ClientHeight);
		AdjustWindowRectEx(&r, CreateStruct.style, CreateStruct.hMenu != NULL, CreateStruct.dwExStyle);
		if(!DEFINED(Width)) {
			width = r.right - r.left;
		}
		if(!DEFINED(Height)) {
			height = r.bottom - r.top;
		}
	}
	if((DEFINED(Width) && DEFINED(Height))
	|| (DEFINED(ClientWidth) && DEFINED(ClientHeight))) {
		Flags |= DIMENSION_DEFINED;
	}
	if(conf->MaintainAspectRatio) {
		if(DEFINED(ClientWidth) && DEFINED(ClientHeight)) {
			Flags |= MAINTAIN_CLIENT_AREA_ASPECT_RATIO;
		} else {
			Flags |= MAINTAIN_FRAME_ASPECT_RATIO;
		}
	}

	if(conf->FullScreen) {
		RECT r;
		r.left = 0;
		r.right = GetSystemMetrics(SM_CXSCREEN);
		r.top = 0;
		r.bottom = GetSystemMetrics(SM_CYSCREEN);
		AdjustWindowRectEx(&r, CreateStruct.style, FALSE, CreateStruct.dwExStyle);

		CreateStruct.x = r.left;
		CreateStruct.y = r.top;
		CreateStruct.cx = r.right - r.left;
		CreateStruct.cy = r.bottom - r.top;
	}
	else {
		CreateStruct.cx = width;
		CreateStruct.x = (conf->Frameless || conf->Type != STANDARD_WINDOW) ? 0 : CW_USEDEFAULT;
		if(conf->HorizAlign == ALIGN_LEFT) {
			// coordinate is an offset from the left edge
			if(DEFINED(Left)) {
				CreateStruct.x = ofRect.left + HPIXEL(Left);
				if(DEFINED(Right)) {
					// positional params have precedence over lengths
					CreateStruct.cx = HPIXEL(Right) - HPIXEL(Left);
				}
			}
			else if(DEFINED(Right)) {
				CreateStruct.x = ofRect.left + HPIXEL(Right) - width;
			}
			else {
				if(conf->Type != STANDARD_WINDOW) {
					CreateStruct.x = ofRect.left;
				}
			}
		}
		else if(conf->HorizAlign == ALIGN_RIGHT) {
			// coordinate is an offset from the right edge
			if(DEFINED(Left)) {
				CreateStruct.x = ofRect.right - HPIXEL(Left);
				if(DEFINED(Right)) {
					CreateStruct.cx = HPIXEL(Left) - HPIXEL(Right);
				}
			}
			else if(DEFINED(Right)) {
				CreateStruct.x = ofRect.right - HPIXEL(Right) - width;
			}
			else {
				CreateStruct.x = ofRect.right - width;
			}
		}
		else {
			// center align the window
			CreateStruct.x = ofRect.left + (ofWidth - width) / 2;
		}

		CreateStruct.cy = height;
		CreateStruct.y = (conf->Frameless || conf->Type != STANDARD_WINDOW || CreateStruct.x != CW_USEDEFAULT) ? 0 : CW_USEDEFAULT;
		if(conf->VertAlign == ALIGN_TOP) {
			// coordinate is an offset from the top
			if(DEFINED(Top)) {
				CreateStruct.y = ofRect.top + VPIXEL(Top);
				if(DEFINED(Bottom)) {
					// positional params have precedence over lengths
					CreateStruct.cy = VPIXEL(Bottom) - VPIXEL(Top);
				}
			}
			else if(DEFINED(Bottom)) {
				CreateStruct.y = ofRect.top + VPIXEL(Bottom) - height;
			}
			else {
				if(conf->Type != STANDARD_WINDOW) {
					CreateStruct.y = ofRect.top;
				}
			}
		}
		else if(conf->VertAlign == ALIGN_BOTTOM) {
			// coordinate is an offset from the bottom edge
			if(DEFINED(Top)) {
				CreateStruct.y = ofRect.bottom - VPIXEL(Top);
				if(DEFINED(Bottom)) {
					CreateStruct.cy = VPIXEL(Top) - VPIXEL(Bottom);
				}
			}
			else if(DEFINED(Bottom)) {
				CreateStruct.y = ofRect.bottom - VPIXEL(Bottom) - height;
			}
			else {
				CreateStruct.y = ofRect.bottom - height;
			}
		}
		else {
			// center align the window
			CreateStruct.y = ofRect.top + (ofHeight - height) / 2;
		}

		// make sure the window doesn't fall off the screen
		if(CreateStruct.x + CreateStruct.cx > waRect.right) {
			CreateStruct.x = waRect.right - CreateStruct.cx;
		}
		if(CreateStruct.y + CreateStruct.cy > waRect.bottom) {
			CreateStruct.y = waRect.bottom - CreateStruct.cy;
		}
		if(CreateStruct.x != CW_USEDEFAULT && CreateStruct.x < waRect.left) {
			CreateStruct.x = waRect.left;
		}
		if(CreateStruct.y != CW_USEDEFAULT && CreateStruct.y < waRect.top) {
			CreateStruct.y = waRect.top;
		}

		// make sure the window has positive height and width
		CreateStruct.cx = max(20, CreateStruct.cx);
		CreateStruct.cy = max(20, CreateStruct.cy);
	}

	if(DEFINED(RoundedCornerX) && DEFINED(RoundedCornerY)) {
		CornerRadii.x = HPIXEL(RoundedCornerX);
		CornerRadii.y = VPIXEL(RoundedCornerY);
	}
	else {
		CornerRadii.x = CornerRadii.y = 0;
	}

	if(DEFINED(MinimumClientWidth)) {
		MinimumSize.cx = HPIXEL(MinimumClientWidth);
	}
	if(DEFINED(MinimumClientHeight)) {
		MinimumSize.cy = VPIXEL(MinimumClientHeight);
	}

	// copy the icon path
	const char *iconPath = "";
	if(conf->IconPath) {
		iconPath = conf->IconPath;
	}
	else if(Application.ClientConf->def_icon_path && conf->Type == STANDARD_WINDOW) {
		iconPath = Application.ClientConf->def_icon_path;
	}
	strncpy(IconPath, iconPath, MAX_PATH);
	IconPath[MAX_PATH] = '\0';	

	CreateStruct.hInstance = _hinstance;
	Application.UnlockApacheConfig(READ_LOCK);
}
#undef DEFINED
#undef HPIXEL
#undef VPIXEL

LRESULT CALLBACK WapacheWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	WapacheWindow *win;
	LRESULT result = 0;
	bool canceled = false;

	if(uMsg == WM_CREATE) {
		CREATESTRUCT *c = reinterpret_cast<CREATESTRUCT *>(lParam);

		SetWindowLong(hwnd, GWL_USERDATA, reinterpret_cast<LONG>(c->lpCreateParams));
		win = reinterpret_cast<WapacheWindow *>(c->lpCreateParams);
		win->Hwnd = hwnd;
		win->AddRef();
		win->OnCreate(c);
		Application.AddRef();
	}
	else {
		win = reinterpret_cast<WapacheWindow *>(GetWindowLong(hwnd, GWL_USERDATA));

		if(win) {
			if(win->Moving) {
				canceled = win->OnMoving(uMsg, wParam, lParam, &result);
			}
			else {
				switch(uMsg) {
				case WM_SIZE:
					win->OnSize(LOWORD(lParam), HIWORD(lParam));
					break;
				case WM_SIZING:
					win->OnSizing(wParam, (RECT *) lParam);
					break;
				case WM_DESTROY:
					win->OnDestroy();
					win->Hwnd = NULL;
					win->Release();
					Application.Release();
					break;
				case WM_COMMAND:
					win->OnCommand(HIWORD(wParam) ? true : false, LOWORD(wParam), reinterpret_cast<HWND>(lParam));
					break;
				case WM_INITMENUPOPUP:
					win->OnInitMenuPopup(reinterpret_cast<HMENU>(wParam), HIWORD(lParam) != 0, LOWORD(lParam));
					break;
				case WM_CLOSE:
					canceled = !win->OnClose(wParam != 0);
					break;
				case WM_SETFOCUS:
					canceled = win->OnSetFocus();
					break;
				case WM_NCACTIVATE:
					canceled = win->OnNCActivate(wParam, (HWND) lParam, &result);
					break;
				case WM_PARENTNOTIFY:
					win->OnParentNotify(wParam, lParam);
					break;
				}
			}
		}
	}
	return (!canceled) ? DefWindowProc(hwnd, uMsg, wParam, lParam) : result;
}

bool WapacheWindow::OnMoving(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *result) 
{
	POINT mousePos, newPos;
	GetCursorPos(&mousePos);
	newPos.x = OriginalPos.x + (mousePos.x - DragPoint.x);
	newPos.y = OriginalPos.y + (mousePos.y - DragPoint.y);
	bool end = false, set = false;

	if(uMsg == WM_MOUSEMOVE) {
		set = true;
	}

	if(uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
		end = true;
	}

	if(uMsg == WM_KEYDOWN) {
		if(wParam == VK_ESCAPE) {
			newPos = OriginalPos;
			set = true;
			end = true;
		}
		else if(wParam == VK_UP) {
			mousePos.y -= 12;
			SetCursorPos(mousePos.x, mousePos.y);
		}
		else if(wParam == VK_DOWN) {
			mousePos.y += 12;
			SetCursorPos(mousePos.x, mousePos.y);
		}
		else if(wParam == VK_RIGHT) {
			mousePos.x += 12;
			SetCursorPos(mousePos.x, mousePos.y);
		}
		else if(wParam == VK_LEFT) {
			mousePos.x -= 12;
			SetCursorPos(mousePos.x, mousePos.y);
		}
	}

	if(uMsg == WM_ACTIVATE) {
		if(LOWORD(wParam) == WA_INACTIVE) {
			end = true;
		}
	}

	if(set) {
		SetWindowPos(Hwnd, NULL, newPos.x, newPos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	if(end) {
		Moving = false;
		ReleaseCapture();
	}
	return false;
}

void WapacheWindow::OnParentNotify(WPARAM wParam, LPARAM lParam) {
	if(wParam == WM_DESTROY) {
		if(lParam) {
			PostMessage(Hwnd, WM_PARENTNOTIFY, wParam, 0);
			return;
		}
		HRESULT hr;
		IHTMLDocument2 *doc;
		IHTMLWindow2 *win;
		IDispatch *docDisp;
		VARIANT_BOOL closed = true;
		hr = Browser->get_Document(&docDisp);
		if(hr == S_OK) {
			hr = docDisp->QueryInterface(&doc);
			docDisp->Release();
		}
		if(hr == S_OK) {
			hr = doc->get_parentWindow(&win);
			doc->Release();
		}
		if(hr == S_OK) {
			hr = win->get_closed(&closed);
			win->Release();
		}
	}
}

void WapacheWindow::OnApplicationStart(void)
{
	wa_initial_url_config **links = (wa_initial_url_config **) Application.ClientConf->initial_urls->elts;
	int count = Application.ClientConf->initial_urls->nelts;
	for(int i = 0; i < count; i++) {
		const char *name = (links[i]->Target) ? links[i]->Target : "";
		const char *openerName = (links[i]->Opener) ? links[i]->Opener : "_app";

		WapacheWindow *win;
		if(OpenUrl(openerName, links[i]->Url, name, &win)) {
			win->Release();
		}
	}
}

void WapacheWindow::OnApplicationEnd(void)
{
	if(WindowList) {
		WapacheWindow *win = WindowList;
        
		do {
			PostMessage(win->Hwnd, WM_CLOSE, 0, 0);
			win = win->Next;
		} while(win != WindowList);
	}
}

void WapacheWindow::OnApacheConfigChange(void)
{
	if(WindowList) {
		WapacheWindow *win = WindowList;
        
		do {
			win->ApplyNewSettings();
			win = win->Next;
		} while(win != WindowList);
	}
}

void WapacheWindow::OnDocumentRootChange(const char *domain) {
	if(WindowList) {
		WapacheWindow *win = WindowList;
		// refresh completely
		VARIANT level;
		V_VT(&level) = VT_I4;
		V_I4(&level) = 3;
        
		do {
			win->Browser->Refresh2(&level);
			win = win->Next;
		} while(win != WindowList);
	}
}

bool WapacheWindow::SetWindowIcon(void)
{
	bool success = false;
	// load the icon
	if(strlen(IconPath) > 0) {
		int smIconX = GetSystemMetrics(SM_CXSMICON);
		int smIconY = GetSystemMetrics(SM_CYSMICON);
		int iconX = GetSystemMetrics(SM_CXICON);
		int iconY = GetSystemMetrics(SM_CXICON);
		HANDLE largeIcon = LoadImage(0, IconPath, IMAGE_ICON, iconX, iconY, LR_LOADFROMFILE);
		HANDLE smallIcon = LoadImage(0, IconPath, IMAGE_ICON, smIconX, smIconY, LR_LOADFROMFILE);
		if(largeIcon && smallIcon) {
			SendMessage(Hwnd, WM_SETICON, ICON_BIG, (LPARAM) largeIcon);
			SendMessage(Hwnd, WM_SETICON, ICON_SMALL, (LPARAM) smallIcon);
			success = true;
		}
	}
	else {
		SendMessage(Hwnd, WM_SETICON, ICON_SMALL, NULL);
		SendMessage(Hwnd, WM_SETICON, ICON_BIG, NULL);
		success = true;
	}
	return success;
}

void WapacheWindow::OnCreate(CREATESTRUCT *c) 
{
	// add window to list
	if(!WindowList) {
		WindowList = this;
		this->AddRef();
		WindowList->Prev =  this;
		this->AddRef();
		WindowList->Next = this;
	}
	else {
		Prev = WindowList->Prev;
		this->AddRef();
		WindowList->Prev->Next = this;
		Next = WindowList;
		this->AddRef();
		WindowList->Prev = this;
	}

	// disable the parent if we're a modal dialog box
	if(Flags & IS_MODAL_DLG) {
		if(Parent) {
			Parent->EnableWindow(FALSE);
		}
	}

	SetWindowIcon();
	
	if(c->style & WS_MAXIMIZE) {
		if(!(Flags & DIMENSION_DEFINED)) {
			ShowWindow(Hwnd, SW_MAXIMIZE);
		}
		else {
			// maximie the window in WM_SIZE so that the
			// initial dimensions are preserved
			Flags |= TO_BE_MAXIMIZED;
		}
	}

	GetClientRect(Hwnd, &ClientRect);

	HRESULT hr;
	hr = CoCreateInstance(CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER, IID_IOleObject, reinterpret_cast<void **>(&Control));
	if(hr == S_OK) {
		IOleClientSite *site = static_cast<IOleClientSite *>(this);
		Control->SetClientSite(site);
		Control->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL, site, 0, Hwnd, &ClientRect);

		hr = Control->QueryInterface(&Browser);
		if(hr == S_OK) {
			Browser->put_RegisterAsBrowser(FALSE);
			hr = Control->QueryInterface(&InPlaceObject);

			IConnectionPointContainer *cpc;
			hr = Browser->QueryInterface(&cpc);
			if(hr == S_OK) {
				IConnectionPoint *cp;
				hr = cpc->FindConnectionPoint(DIID_DWebBrowserEvents, &cp);
				if(hr == S_OK) {
					IDispatch *disp = static_cast<IDispatch *>(this);
					hr = cp->Advise(static_cast<IUnknown *>(disp), &AdviseConn);
					cp->Release();
				}
				hr = cpc->FindConnectionPoint(DIID_DWebBrowserEvents2, &cp);
				if(hr == S_OK) {
					IDispatch *disp = static_cast<IDispatch *>(this);
					hr = cp->Advise(static_cast<IUnknown *>(disp), &AdviseConn2);
					cp->Release();
				}
				cpc->Release();
			}
		}
	}

	if(FAILED(hr)) {
		DestroyWindow(Hwnd);
	}
}

bool WapacheWindow::OnClose(bool mustClose)
{
	bool can_close = true;
	if(!mustClose) {
		if(this != LastWindowToClose) {
			HRESULT hr;
			IHTMLDocument2 *doc;
			IDispatch *docDisp;
			hr = Browser->get_Document(&docDisp);
			if(hr == S_OK) {
				hr = docDisp->QueryInterface(&doc);
				docDisp->Release();
			}
			if(hr == S_OK) {
				WapacheExecContext e;
				InitExecContext(&e, doc);
				can_close = ExecOnCloseHandler(&e);
				ClearExecContext(&e);

				if(!can_close && LastWindowToClose) {
					LastWindowToClose = NULL;
				}
			}
		}
		// in case ExitProgram is called within onclose
		if(this == LastWindowToClose) {
			can_close = false;
		}
	}

	if(can_close) {
		// reenable the parent if we're a modal dialog box
		if(Flags & IS_MODAL_DLG) {
			if(Parent) {
				Parent->EnableWindow(TRUE);
			}
		}
	}
	return can_close;
}

bool WapacheWindow::ExecOnCloseHandler(WapacheExecContext *context) {
	bool canClose = true;
	Application.LockApacheConfig(READ_LOCK);
	wa_win_config *conf = Application.GetWindowSettings(Name);
	wa_onclose_config *onclose = conf->OnClose;
	if(onclose) {
		int argc;
		const char **argv;
		VARIANT *argvV;
		if(onclose->Args) {
			argv = reinterpret_cast<const char **>(onclose->Args->elts);
			argc = onclose->Args->nelts;

			Application.LockApacheEnv(READ_LOCK);
			argvV = new VARIANTARG[argc];
			for(int i = 0, j = argc - 1; i < argc; i++, j--) {
				const char *arg = Application.ExpandEnvironmentStr(context->TemporaryPool, argv[i], context->Domain);
				VariantInit(&argvV[j]);
				V_VT(&argvV[j]) = VT_BSTR;
				V_BSTR(&argvV[j]) = Application.ConvertString(context->Codepage, arg, -1);
			}
			Application.UnlockApacheEnv(READ_LOCK);
		} else {
			argc = 0;
			argvV = NULL;
		}
		int resultCount; 
		VARIANT *results;
		context->Pattern = onclose->Pattern;
		ExecJSMethod(context, onclose->Method, argvV, argc, &results, &resultCount);
		for(int i = 0; i < resultCount; i++) {
			VARIANT b;
			VariantInit(&b);
			VariantChangeType(&b, &results[i], 0, VT_BOOL);
			VariantClear(&results[i]);
			if(V_BOOL(&b) == VARIANT_FALSE) {
				canClose = false;
			}
		}
	}
	Application.UnlockApacheConfig(READ_LOCK);
	return canClose;
}

void WapacheWindow::OnDestroy(void) 
{
	WapacheWindow *win = WindowList;
	do {
		if(win->Parent == this) {
			this->Release();
			win->Parent = NULL;
		}
		win = win->Next;
	} while(win != WindowList);

	if(WindowList == this) {
		WindowList = Next;
	}
	Prev->Next = Next;
	this->Release();
	Next->Prev = Prev;
	this->Release();
	Next = Prev = NULL;
	if(WindowList == this) {
		WindowList = NULL;
	}
	if(LastWindowToClose) {
		CloseNextWindow();
	}
}

void WapacheWindow::OnSizing(WORD type, RECT *rect) {
	long frame_width = rect->right - rect->left;
	long frame_height = rect->bottom - rect->top;
	switch(type) {
		case WMSZ_TOP:
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
			rect->top = rect->bottom - max(frame_height, MinimumSize.cy + FrameOffset.cy);
		break;
		case WMSZ_BOTTOM:
		case WMSZ_BOTTOMRIGHT:
		case WMSZ_BOTTOMLEFT:
			rect->bottom = rect->top + max(frame_height, MinimumSize.cy + FrameOffset.cy);
		break;
	}
	switch(type) {
		case WMSZ_LEFT:
		case WMSZ_TOPLEFT:
		case WMSZ_BOTTOMLEFT:
			rect->left = rect->right - max(frame_width, MinimumSize.cx + FrameOffset.cx);
		break;
		case WMSZ_RIGHT:
		case WMSZ_TOPRIGHT:
		case WMSZ_BOTTOMRIGHT:
			rect->right = rect->left + max(frame_width, MinimumSize.cx + FrameOffset.cx);
		break;
	}
	if(Flags & MAINTAIN_ASPECT_RATIO) {
		long prop_width, prop_height;
		if(Flags & MAINTAIN_CLIENT_AREA_ASPECT_RATIO) {
			long client_width = frame_width - FrameOffset.cx;
			long client_height = frame_height - FrameOffset.cy;
			prop_width = (long) (client_height * AspectRatio) + FrameOffset.cx;
			prop_height = (long) (client_width / AspectRatio) + FrameOffset.cy;
		} else {
			prop_width = (long) (frame_width * AspectRatio);
			prop_height = (long) (frame_height / AspectRatio);
		}
		if(prop_width < MinimumSize.cx) {
			prop_width = MinimumSize.cx;
		}
		if(prop_height < MinimumSize.cy) {
			prop_height = MinimumSize.cy;
		}
		switch(type) {
			case WMSZ_TOP:
			case WMSZ_BOTTOM:
				rect->right = rect->left + prop_width;
			break;
			case WMSZ_LEFT:
			case WMSZ_RIGHT:
				rect->bottom = rect->top + prop_height;
			break;
			case WMSZ_BOTTOMRIGHT:
			case WMSZ_BOTTOMLEFT:
				rect->bottom = rect->top + prop_height;
			break;
			case WMSZ_TOPLEFT:
			case WMSZ_TOPRIGHT:
				rect->top = rect->bottom - prop_height;
			break;
		}
	}
}

void WapacheWindow::OnSize(WORD width, WORD height)
{ 
	GetClientRect(Hwnd, &ClientRect);
	if(!AspectRatio) {
		RECT windowRect;
		SIZE windowSize, clientSize;
		GetWindowRect(Hwnd, &windowRect);

		windowSize.cx = windowRect.right - windowRect.left;
		windowSize.cy = windowRect.bottom - windowRect.top;
		clientSize.cx = ClientRect.right - ClientRect.left;
		clientSize.cy = ClientRect.bottom - ClientRect.top;

		if(Flags & MAINTAIN_CLIENT_AREA_ASPECT_RATIO) {
			AspectRatio =  (double) clientSize.cx / clientSize.cy;

		} else {
			AspectRatio =  (double) windowSize.cx / windowSize.cy;
		}
		FrameOffset.cx = windowSize.cx - clientSize.cx;
		FrameOffset.cy = windowSize.cy - clientSize.cy;
	}
	if(Control) {
		HRESULT hr;
		IOleInPlaceObject *ip;
		hr = Control->QueryInterface(&ip);
		if(hr == S_OK) {
			ip->SetObjectRects(&ClientRect, &ClientRect);
			ip->Release();
		}
	}
	if(Flags & TO_BE_MAXIMIZED) {
		Flags &= ~TO_BE_MAXIMIZED;
		ShowWindow(Hwnd, SW_MAXIMIZE);
	}
	else {
		if(CornerRadii.x > 0 && CornerRadii.y > 0) {
			HRGN rgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, CornerRadii.x, CornerRadii.y);
			SetWindowRgn(Hwnd, rgn, false);
		}
	}
}

void WapacheWindow::OnCommand(bool fromAccel, UINT cmdId, HWND control)
{
	HRESULT hr;
	IDispatch *docDisp;
	IHTMLDocument2 *doc;
	hr = Browser->get_Document(&docDisp);
	if(hr == S_OK) {
		hr = docDisp->QueryInterface(&doc);
		docDisp->Release();
	}
	if(hr == S_OK) {
		HMENU hMenu = GetMenu(Hwnd);
		WapacheExecContext e;
		InitExecContext(&e, doc);
		ExecMenuCommand(&e, hMenu, cmdId);
		ClearExecContext(&e);
		doc->Release();
	}
}

bool AmongOwners(HWND b, HWND a) {
	HWND p = GetParent(a);
	if(a == b) {
		return true;
	}
	else if(p) {
		return AmongOwners(p, b);
	}
	return false;
}

void WapacheWindow::EnableWindow(BOOL enabled) 
{
	// enable/disable our tool windows as well
	WapacheWindow *win = WindowList;
	do {
		if(win->Parent == this) {
			if(win->Flags & IS_TOOL_WIN) {
				::EnableWindow(win->Hwnd, enabled);
			}
		}
		win = win->Next;
	} while(win != WindowList);
	::EnableWindow(Hwnd, enabled);
}

void WapacheWindow::ActivateWindow(UINT state)
{
	// show our tool windows as active/inactive as well
	WapacheWindow *win = WindowList;
	do {
		if(win->Parent == this) {
			if(win->Flags & IS_TOOL_WIN) {
				win->ActivateWindow(state);
			}
		}
		win = win->Next;
	} while(win != WindowList);
	DefWindowProc(Hwnd, WM_NCACTIVATE, state, 0);	
}

bool WapacheWindow::OnSetFocus(void) {
	if(InPlaceObject) {
		InPlaceObject->OnFrameWindowActivate(true);
		InPlaceObject->OnDocWindowActivate(true);
	}	
	return false;
}

bool WapacheWindow::OnNCActivate(UINT state, HWND otherHwnd, LRESULT *result) {
	WapacheWindow *root;
	// the base window always handle the activation
	for(root = this; root->Parent; root = root->Parent) {
		if(!(root->Flags & IS_TOOL_WIN)) {
			break;
		}
	}
	root->ActivateWindow(state);
	return false;
}

void WapacheWindow::OnInitMenuPopup(HMENU hMenu, bool isWindowMenu, UINT pos)
{
	if(!isWindowMenu) {
		HRESULT hr;
		IDispatch *docDisp = NULL;
		IHTMLDocument2 *doc = NULL;
		hr = Browser->get_Document(&docDisp);
		if(docDisp) {
			hr = docDisp->QueryInterface(&doc);
			docDisp->Release();
		}
		if(doc) {
			WapacheExecContext e;
			InitExecContext(&e, doc);
			SetMenuItemAvailability(&e, hMenu);
			ClearExecContext(&e);
			doc->Release();
		}
	}
}

int WapacheWindow::ProcessKeyboardEvent(MSG *msg)
{
	HWND parent = GetAncestor(msg->hwnd, GA_ROOT);
	HRESULT hr = S_FALSE;
	WapacheWindow *win = WindowList;
	do {
		if(win->Hwnd == parent) {
			hr = win->InPlaceObject->TranslateAccelerator(msg);
			break;
		}
		win = win->Next;
	} while(win != WindowList);
	return (hr == S_OK);
}

void WapacheWindow::OnBeforeNavigate(          
	IDispatch *pDisp,
	VARIANT *&url,
	VARIANT *&Flags,
	VARIANT *&TargetFrameName,
	VARIANT *&PostData,
	VARIANT *&Headers,
	VARIANT_BOOL *&Cancel)
{
	if(Application.IsInternalAddress(V_BSTR(url))) {
		WapacheProtocol::SetBrowser(pDisp);
	}
	else {
		char *cmd = WideStringToCStr(V_BSTR(url), -1);
		ShellExecute(Hwnd, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
		delete[] cmd;
		*Cancel = true;
	}
}

void WapacheWindow::OnDocumentComplete(          
	IDispatch *pDisp,
	VARIANT *URL)
{
	if(Flags & UNNAMED) {
		HRESULT hr;
		IDispatch *docDisp = NULL;
		IHTMLDocument2 *doc = NULL;
		IHTMLWindow2 *win = NULL;
		hr = Browser->get_Document(&docDisp);
		if(docDisp) {
			hr = docDisp->QueryInterface(&doc);
			docDisp->Release();
		}
		if(doc) {
			doc->get_parentWindow(&win);
			doc->Release();
		}
		if(win) {
			LPOLESTR nameW = CStrToWideString(Name, -1);
			hr = win->put_name(nameW);
			SysFreeString(nameW);
			win->Release();
		}
		Flags &= ~UNNAMED;
	}

	HWND hwnd, inner;
	InPlaceObject->GetWindow(&hwnd);
	do {
		inner = hwnd;
		hwnd = ::GetWindow(hwnd, GW_CHILD);
	} while(hwnd);
	::SetFocus(inner);
}

void WapacheWindow::OnNavigateComplete(          
	IDispatch *pDisp,
	VARIANT *URL)
{
}

void WapacheWindow::OnNavigateError(
	IDispatch *pDisp,
	VARIANT *URL,
	VARIANT *TargetFrameName,
	VARIANT *StatusCode,
	VARIANT_BOOL *&Cancel)
{
}

void WapacheWindow::OnNewWindow(          
	IDispatch **&ppDisp,
	BSTR Url,
	DWORD Flags,
	BSTR TargetFrameName,
	VARIANT *&PostData,
	BSTR Headers,
	VARIANT_BOOL *&Cancel)
{
	// don't open new window for external links
	if(!Application.IsInternalAddress(Url)) {
		char *cmd = WideStringToCStr(Url, -1);
		ShellExecute(Hwnd, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
		delete[] cmd;
		*Cancel = true;
		return;
	}

	// IE 5.5+ adds a random string like _[343] to the target name
	// (security fix of some sort?)
	LPOLESTR nameW = wcschr(TargetFrameName, ']');
	if(nameW) {
		nameW++;
	}
	else {
		nameW = TargetFrameName;
	}

	// when target is "_blank"
	if(wcsncmp(nameW, L"_No__Name", 8) == 0) {
		nameW = NULL;
	}

	WapacheWindow *win = NULL;
	char *name = WideStringToCStr(nameW, -1);

	if(nameW) {
		VARIANT flagsV;
		VARIANT nameV;
		VARIANT headersV;

		V_VT(&flagsV) = VT_I4; V_I4(&flagsV) = Flags;
		V_VT(&nameV) = VT_BSTR; V_BSTR(&nameV) = nameW;
		V_VT(&headersV) = VT_BSTR; V_BSTR(&headersV) = Headers;

		// look at the window's own frames first
		if(HasFrame(Browser, nameW)) {
			//Browser->Navigate(Url, FlagsV, nameV, *postDataV, headersV);
			*Cancel = false;
			return;
		}

		// look for a top level window with that name
		if(FindWindow(name, &win)) {
			win->Browser->AddRef();
			win->Focus();
			win->Release();
			*ppDisp = win->Browser;	
			delete[] name;
			return;
		}
		if(FindWindowWithFrame(name, &win)) {
			win->Browser->Navigate(Url, &flagsV, &nameV, PostData, &headersV);

			delete[] name;
			*Cancel = true;
			return;
		}
	}

	// can't find window--create it
	win = new WapacheWindow((name) ? name : "", this);
	if(win->Start()) {
		win->Browser->AddRef();
		*ppDisp = win->Browser;	
	}
	else {
		*Cancel = true;
	}
	delete[] name;
}

void WapacheWindow::OnTitleChange(BSTR title)
{
	SetWindowTextW(Hwnd, title);
}

void WapacheWindow::OnStatusTextChange(BSTR status)
{
}

HRESULT STDMETHODCALLTYPE WapacheWindow::QueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if(riid == IID_IUnknown) {
		*ppvObject = static_cast<IUnknown *>( static_cast<IDispatch *>(this) );
	}
	else if(riid == IID_IDispatch) {
		*ppvObject = static_cast<IDispatch *>(this);
	}
	else if(riid == IID_IServiceProvider) {
		*ppvObject = static_cast<IServiceProvider *>(this);
	}
	else if(riid == IID_IOleClientSite) {
		*ppvObject = static_cast<IOleClientSite *>(this);
	}
	else if(riid == IID_IOleInPlaceSite) {
		*ppvObject = static_cast<IOleInPlaceSite *>(this);
	}
	else if(riid == IID_IOleInPlaceFrame) {
		*ppvObject = static_cast<IOleInPlaceFrame *>(this);
	}
	else if(riid == IID_IDocHostUIHandler) {
		*ppvObject = static_cast<IDocHostUIHandler *>(this);
	}
	else if(riid == IID_IDocHostUIHandler2) {
		*ppvObject = static_cast<IDocHostUIHandler2 *>(this);
	}
	else if(riid == IID_IInternetSecurityManager) {
		*ppvObject = static_cast<IInternetSecurityManager *>(this);
	}
	else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}
    
ULONG STDMETHODCALLTYPE WapacheWindow::AddRef(void)
{
	return ++RefCount;
}
    
ULONG STDMETHODCALLTYPE WapacheWindow::Release(void)
{
	if(--RefCount == 0) {
		delete this;
		return 0;
	}
	return RefCount;
}


HRESULT STDMETHODCALLTYPE WapacheWindow::GetTypeInfoCount( 
    /* [out] */ UINT __RPC_FAR *pctinfo)
{
	*pctinfo = 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetTypeInfo( 
    /* [in] */ UINT iTInfo,
    /* [in] */ LCID lcid,
    /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
{
	*ppTInfo = NULL;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetIDsOfNames( 
    /* [in] */ REFIID riid,
    /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
    /* [in] */ UINT cNames,
    /* [in] */ LCID lcid,
    /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::Invoke( 
    /* [in] */ DISPID dispIdMember,
    /* [in] */ REFIID riid,
    /* [in] */ LCID lcid,
    /* [in] */ WORD wFlags,
    /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
    /* [out] */ VARIANT __RPC_FAR *pVarResult,
    /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
    /* [out] */ UINT __RPC_FAR *puArgErr)
{
	VARIANTARG *argv = pDispParams->rgvarg;
	int argc = pDispParams->cArgs;
	HRESULT hr = DISP_E_MEMBERNOTFOUND;
	static IDispatch **Cow;

	switch(dispIdMember) {
	case DISPID_BEFORENAVIGATE2:
		if(argc == 7 && V_VT(&argv[6]) == VT_DISPATCH
					 && V_VT(&argv[5]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[4]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[3]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[2]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[1]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[0]) == (VT_BOOL | VT_BYREF)) {
			OnBeforeNavigate(V_DISPATCH(&argv[6]),
							 V_VARIANTREF(&argv[5]),
							 V_VARIANTREF(&argv[4]),
							 V_VARIANTREF(&argv[3]),
							 V_VARIANTREF(&argv[2]),
							 V_VARIANTREF(&argv[1]),
							 V_BOOLREF(&argv[0]));
			hr = S_OK;
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_NAVIGATECOMPLETE2:
		if(argc == 2 && V_VT(&argv[1]) == VT_DISPATCH
					 && V_VT(&argv[0]) == (VT_VARIANT | VT_BYREF)) {
			OnNavigateComplete(
				V_DISPATCH(&argv[1]),
				&argv[0]);
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_NAVIGATEERROR:
		if(argc == 5 && V_VT(&argv[4]) == VT_DISPATCH
					 && V_VT(&argv[3]) == VT_VARIANT
					 && V_VT(&argv[2]) == VT_VARIANT
					 && V_VT(&argv[1]) == VT_VARIANT
					 && V_VT(&argv[0]) == (VT_BOOL | VT_BYREF)) {
			OnNavigateError(
				V_DISPATCH(&argv[4]),
				&argv[3],
				&argv[2],
				&argv[1],
				V_BOOLREF(&argv[0]));
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_TITLECHANGE:
		if(argc == 1 && V_VT(&argv[0]) == VT_BSTR) {
			OnTitleChange(V_BSTR(&argv[0]));
			hr = S_OK;
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_NEWWINDOW:
		if(argc == 6 && V_VT(&argv[5]) == VT_BSTR
					 && V_VT(&argv[4]) == VT_I4
					 && V_VT(&argv[3]) == VT_BSTR
					 && V_VT(&argv[2]) == (VT_VARIANT | VT_BYREF)
					 && V_VT(&argv[1]) == VT_BSTR
					 && V_VT(&argv[0]) == (VT_BOOL | VT_BYREF)) {
			/* whoa, ugly hack! */
			if(you_dont_see_me && !*you_dont_see_me) {
				OnNewWindow(you_dont_see_me,
							V_BSTR(&argv[5]),
							V_I4(&argv[4]),
							V_BSTR(&argv[3]),
							V_VARIANTREF(&argv[2]),
							V_BSTR(&argv[1]),
							V_BOOLREF(&argv[0]));
				you_dont_see_me = NULL;
			}
			hr = S_OK;
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_NEWWINDOW2:
		if(argc == 2 && V_VT(&argv[1]) == (VT_DISPATCH | VT_BYREF)
					 && V_VT(&argv[0]) == (VT_BOOL | VT_BYREF)) {
			you_dont_see_me = V_DISPATCHREF(&argv[1]);
		}
		break;
	case DISPID_DOCUMENTCOMPLETE:
		if(argc == 2 && V_VT(&argv[1]) == VT_DISPATCH
					 && V_VT(&argv[0]) == (VT_VARIANT | VT_BYREF)) {
			OnDocumentComplete(V_DISPATCH(&argv[1]),
							   V_VARIANTREF(&argv[0]));
			hr = S_OK;
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_STATUSTEXTCHANGE:
		if(argc == 1 && V_VT(&argv[0]) == VT_BSTR) {
			OnStatusTextChange(V_BSTR(&argv[0]));
		}
		else {
			hr = DISP_E_BADVARTYPE;
		}
		break;
	case DISPID_WINDOWCLOSING:
		Close(true);
		break;
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::QueryService( 
    /* [in] */ REFGUID guidService,
    /* [in] */ REFIID riid,
    /* [out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	HRESULT hr = E_NOINTERFACE;

	*ppvObject = NULL;
	if(guidService == SID_SInternetSecurityManager) {
		hr = QueryInterface(riid, ppvObject);
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SaveObject(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetMoniker( 
    /* [in] */ DWORD dwAssign,
    /* [in] */ DWORD dwWhichMoniker,
    /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmk)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetContainer( 
    /* [out] */ IOleContainer __RPC_FAR *__RPC_FAR *ppContainer)
{
	*ppContainer = NULL;
	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::ShowObject(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnShowWindow( 
    /* [in] */ BOOL fShow)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::RequestNewObjectLayout(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetWindow( 
    /* [out] */ HWND __RPC_FAR *phwnd)
{
	if(!Hwnd) {
		return E_FAIL;
	}
	*phwnd = Hwnd;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::ContextSensitiveHelp( 
    /* [in] */ BOOL fEnterMode)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::CanInPlaceActivate(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnInPlaceActivate(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnUIActivate(void)
{
	return S_OK;
}


HRESULT STDMETHODCALLTYPE WapacheWindow::GetWindowContext( 
    /* [out] */ IOleInPlaceFrame __RPC_FAR *__RPC_FAR *ppFrame,
    /* [out] */ IOleInPlaceUIWindow __RPC_FAR *__RPC_FAR *ppDoc,
    /* [out] */ LPRECT lprcPosRect,
    /* [out] */ LPRECT lprcClipRect,
    /* [out] */ LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	AddRef();
	*ppFrame = static_cast<IOleInPlaceFrame *>(this);
	AddRef();
	*ppDoc = static_cast<IOleInPlaceUIWindow *>(this);

	*lprcPosRect = ClientRect;
	*lprcClipRect = ClientRect;

	if(lpFrameInfo->cb == sizeof(OLEINPLACEFRAMEINFO)) {
		lpFrameInfo->fMDIApp = FALSE;
		lpFrameInfo->hwndFrame = Hwnd;
		lpFrameInfo->haccel = NULL;
		lpFrameInfo->cAccelEntries = 0;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::Scroll( 
    /* [in] */ SIZE scrollExtant)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnUIDeactivate( 
    /* [in] */ BOOL fUndoable)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnInPlaceDeactivate(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::DiscardUndoState(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::DeactivateAndUndo(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnPosRectChange( 
    /* [in] */ LPCRECT lprcPosRect)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::InsertMenus( 
    /* [in] */ HMENU hmenuShared,
    /* [out][in] */ LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
	return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetMenu( 
    /* [in] */ HMENU hmenuShared,
    /* [in] */ HOLEMENU holemenu,
    /* [in] */ HWND hwndActiveObject)
{
	return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::RemoveMenus( 
    /* [in] */ HMENU hmenuShared)
{
	return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetStatusText( 
    /* [unique][in] */ LPCOLESTR pszStatusText)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::EnableModeless( 
    /* [in] */ BOOL fEnable)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::TranslateAccelerator( 
    /* [in] */ LPMSG lpmsg,
    /* [in] */ WORD wID)
{
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetBorder( 
    /* [out] */ LPRECT lprectBorder)
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::RequestBorderSpace( 
    /* [unique][in] */ LPCBORDERWIDTHS pborderwidths)
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetBorderSpace( 
    /* [unique][in] */ LPCBORDERWIDTHS pborderwidths)
{
	return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetActiveObject( 
    /* [unique][in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
    /* [unique][string][in] */ LPCOLESTR pszObjName)
{
	return S_OK;
}

bool CheckMenuItemAvailability(HMENU hMenu, int pos, int id, void *data) {
	WapacheExecContext *context = (WapacheExecContext *) data;
	UINT enableFlags = MF_BYPOSITION | MF_GRAYED;
	UINT checkFlags = MF_BYPOSITION | MF_UNCHECKED;
	if(id >= SCRIPT_MENU_ID_START) { 
		MENUITEMINFO mii;
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_DATA;
		if(GetMenuItemInfo(hMenu, pos, TRUE, &mii)) {
			IDispatch *itemDisp = (IDispatch *) mii.dwItemData;
			if(itemDisp) {
				VARIANT disabled, method, checked;
				VariantInit(&disabled);
				VariantInit(&method);
				VariantInit(&checked);
				if(GetProperty(itemDisp, L"method", &method, VT_DISPATCH)) {
					enableFlags = MF_BYPOSITION | MF_ENABLED;
					VariantClear(&method);
				}
				if(GetProperty(itemDisp, L"disabled", &disabled, VT_BOOL)) {
					if(V_BOOL(&disabled) == VARIANT_TRUE) {
						enableFlags = MF_BYPOSITION | MF_GRAYED;
					}
					VariantClear(&disabled);
				}
				if(GetProperty(itemDisp, L"checked", &checked, VT_BOOL)) {
					if(V_BOOL(&checked) == VARIANT_TRUE) {
						checkFlags = MF_BYPOSITION | MF_CHECKED;
					}
					VariantClear(&checked);
				}
			}
		}				
	} else {
		wa_menu_item_config *item = Application.FindMenuItem(id);
		if(item) {
			context->Flags = TEST_DONT_EXEC;
			context->Result = 0;
			WapacheWindow::ExecMenuCommand(context, item);
			if(context->Result & ITEM_AVAILABLE) {
				enableFlags = MF_BYPOSITION | MF_ENABLED;
			}
			if(context->Result & ENV_SET) {
				enableFlags = MF_BYPOSITION | MF_DISABLED;
				checkFlags = MF_BYPOSITION | MF_CHECKED;
			}
		}
	}
	EnableMenuItem(hMenu, pos, enableFlags);
	CheckMenuItem(hMenu, pos, checkFlags);
	return true;
}

void WapacheWindow::SetMenuItemAvailability(WapacheExecContext *context, HMENU hMenu) 
{
	Application.LockApacheConfig(READ_LOCK);

	EnumMenuRecursive(hMenu, CheckMenuItemAvailability, context);

	Application.UnlockApacheConfig(READ_LOCK);
}

bool WapacheWindow::CheckExecContext(WapacheExecContext *context)
{
	apr_pool_t *pTemp = context->TemporaryPool;

	HRESULT hr;
	BSTR nameW = NULL, urlW = NULL;
	char *name = NULL, *url = NULL; 
	UrlComponents loc;
	hr = context->Win->get_name(&nameW);
	if(hr == S_OK) {
		name = WideStringToPoolCStr(pTemp, nameW, -1);
	}
	if(!name) {
		name = "";
	}
	SysFreeString(nameW);
	hr = context->Doc->get_URL(&urlW);
	if(hr == S_OK) {
		url = WideStringToPoolCStr(pTemp, urlW, -1);
	}
	else {
		return false;
	}
	SysFreeString(urlW);

	if(!ParseUrl(pTemp, &loc, url)) {
		return false;
	}

	if(context->Pattern) {
		if(ap_regexec(context->Pattern, "_this", 0, NULL, 0) == 0) {
			if(context->Win != context->OriginWin) {
				return false;
			}
		}
		else if(ap_regexec(context->Pattern, "_other", 0, NULL, 0) == 0) {
			if(context->Win == context->OriginWin) {
				return false;
			}
		}
		else if(ap_regexec(context->Pattern, "_opener", 0, NULL, 0) == 0) {
			if(!context->OriginWin) {
				return false;
			}
			HRESULT hr;
			VARIANT opener;
			IHTMLWindow2 *openerWin = NULL;
			IHTMLDocument2 *openerDoc;
			hr = context->OriginWin->get_opener(&opener);
			if(hr == S_OK) {
				if(V_VT(&opener) == VT_DISPATCH) {
					hr = V_DISPATCH(&opener)->QueryInterface(&openerWin);
				}
				else {
					hr = E_INVALIDARG;
				}
				VariantClear(&opener);
			}
			// need to do this because the interface from get_opener is actually
			// different from the one return by get_parentWindow
			if(hr == S_OK) {
				hr = openerWin->get_document(&openerDoc);
				openerWin->Release();
			}
			if(hr == S_OK) {
				hr = openerDoc->get_parentWindow(&openerWin);
				openerDoc->Release();
			}
			if(hr == S_OK) {
				openerWin->Release();
			}
			if(context->Win != openerWin) {
				return false;
			}
		}
		else if(ap_regexec(context->Pattern, "_parent", 0, NULL, 0) == 0) {
			if(!context->OriginWin) {
				return false;
			}
			HRESULT hr;
			IHTMLWindow2 *parentWin;
			hr = context->OriginWin->get_parent(&parentWin);
			if(context->Win != parentWin) {
				return false;
			}
		}
		else if(ap_regexec(context->Pattern, name, 0, NULL, 0) == 0) {
		}
		else if(ap_regexec(context->Pattern, "_all", 0, NULL, 0) != 0) {
			return false;
		}
	}
	else {
		if(strlen(name) != 0) {
			return false;
		}
	}

	if(!(context->Flags & DONT_CHECK_SCHEME)) {
		// check the scheme
		if(stricmp(context->Scheme, loc.Scheme) != 0) {
			return false;
		}
	}

	if(!(context->Flags & DONT_CHECK_DOMAIN)) {
		// check the domain
		if(stricmp(context->Domain, loc.Host) != 0) {
			return false;
		}
	}

	return true;
}

void WapacheWindow::Dispatch(WapacheExecContext *context, IWebBrowser2 *browser)
{
	HRESULT hr;
	IDispatch *docDisp;
	hr = browser->get_Document(&docDisp);
	if(hr == S_OK) {
		IHTMLDocument2 *doc;
		IHTMLWindow2 *win;
		hr = docDisp->QueryInterface(&doc);
		if(hr == S_OK) {
			hr = doc->get_parentWindow(&win);
			doc->Release();
		}
		if(hr == S_OK) {
			hr = win->get_document(&doc);
			if(hr == S_OK) {
				context->Browser = browser;
				context->Win = win;
				context->Doc = doc;
				if(CheckExecContext(context)) {
					context->Function(context);
				}
				doc->Release();
			}
			win->Release();
		}

		// do the same thing for all frames
		IOleContainer *container;
		IEnumUnknown *enumerator;
		hr = docDisp->QueryInterface(&container);
		if(hr == S_OK) {
			hr = container->EnumObjects(OLECONTF_EMBEDDINGS, &enumerator);
			container->Release();
		}
		if(hr == S_OK) {
			IUnknown *unknown;
			while(enumerator->Next(1, &unknown, NULL) == S_OK) {
				IWebBrowser2* frame;
				hr = unknown->QueryInterface(&frame);
				if(hr == S_OK) {
					Dispatch(context, frame);
					frame->Release();
				}
				unknown->Release();
			}
			enumerator->Release();
		}
		docDisp->Release();
	}
}

bool WapacheWindow::FindWindow(const char *name, WapacheWindow **pWin)
{
	if(WindowList) {
		WapacheWindow *win = WindowList;
		do {
			if(stricmp(win->Name, name) == 0) {
				win->AddRef();
				*pWin = win;
				return true;
			}
			win = win->Next;
		} while(win != WindowList);
	}
	return false;
}

bool WapacheWindow::FindWindowWithFrame(const char *name, WapacheWindow **pWin)
{
	BSTR targetW = CStrToWideString(name, -1);
	if(WindowList) {
		WapacheWindow *win = WindowList;
		do {
			if(HasFrame(win->Browser, targetW)) {
				win->AddRef();
				*pWin = win;
				SysFreeString(targetW);
				return true;
			}
			win = win->Next;
		} while(win != WindowList);
	}
	SysFreeString(targetW);
	return false;
}

bool WapacheWindow::HasFrame(IWebBrowser2 *browser, BSTR target)
{
	HRESULT hr;
	bool hasFrame = false;
	IDispatch *docDisp = NULL;
	hr = browser->get_Document(&docDisp);
	if(docDisp) {
		IOleContainer *container = NULL;
		IEnumUnknown *enumerator = NULL;
		hr = docDisp->QueryInterface(&container);
		if(container) {
			hr = container->EnumObjects(OLECONTF_EMBEDDINGS, &enumerator);
			container->Release();
		}
		if(enumerator) {
			IUnknown *unknown = NULL;
			while(!hasFrame && (enumerator->Next(1, &unknown, NULL) == S_OK)) {
				IWebBrowser2* frame = NULL;
				hr = unknown->QueryInterface(&frame);
				if(frame) {
					IHTMLFrameBase *base = NULL;
					hr = unknown->QueryInterface(&base);
					if(base) {
						BSTR nameW = NULL;
						base->get_name(&nameW);
						base->Release();
						if(nameW) {
							if(wcsicmp(nameW, target) == 0) {
								hasFrame = true;
							}
							SysFreeString(nameW);
						}
					}
					if(hasFrame) {
						HasFrame(frame, target);
					}
					frame->Release();
				}
				unknown->Release();
			}
			enumerator->Release();
		}
		docDisp->Release();
	}
	return hasFrame;
}

bool WapacheWindow::OpenUrl(const char *openerName, const char *url, const char *target, WapacheWindow **pWin)
{
	*pWin = NULL;
	LPWSTR urlW = CStrToWideString(url, -1);

	// find the opener
	if(stricmp(openerName, "_app") == 0) {
		if(FindWindow(target, pWin) || FindWindowWithFrame(target, pWin)) {
			(*pWin)->Focus();
		}
		else {
			*pWin = new WapacheWindow(target, NULL);
			if(!(*pWin)->Start()) {
				delete *pWin;
				*pWin = NULL;
			}
		}
		if(*pWin) {
			(*pWin)->Browser->Navigate(urlW, NULL, NULL, NULL, NULL);
		}
	}
	else {
		if(FindWindow(openerName, pWin)) {
			VARIANT targetV;
			VariantInit(&targetV);
			V_VT(&targetV) = VT_BSTR;
			V_BSTR(&targetV) = CStrToWideString(target, -1);
			(*pWin)->Browser->Navigate(urlW, NULL, &targetV, NULL, NULL);
			VariantClear(&targetV);
		}
	}

	SysFreeString(urlW);

	return (*pWin != NULL);
}

void WapacheWindow::Broadcast(WapacheExecContext *context) 
{
	if(WindowList) {
		WapacheWindow *win = WindowList;
		do {
			context->WinObj = win;
			Dispatch(context, win->Browser);
			if(context->Aborted) {
				break;
			}
			win = win->Next;
		} while(win != WindowList);
	}
}

void WapacheWindow::InitExecContext(WapacheExecContext *context, IHTMLDocument2 *originDoc)
{
	HRESULT hr;
	ZeroMemory(context, sizeof(WapacheExecContext));

	// create a temporary pool for string operations
	apr_pool_create(&context->TemporaryPool, Application.Process->pool);

	// parse the URL of the origin doc to see in which windows the
	// operation should proceed and how to expand environment strings
	BSTR oUrlW;
	hr = originDoc->get_URL(&oUrlW);
	if(hr == S_OK) {
		char *oUrl = WideStringToPoolCStr(context->TemporaryPool, oUrlW, -1);
		UrlComponents oLoc;
		if(ParseUrl(context->TemporaryPool, &oLoc, oUrl)) {
			context->Scheme = oLoc.Scheme;
			context->Domain = oLoc.Host;
		}
		SysFreeString(oUrlW);
	}

	hr = originDoc->get_parentWindow(&context->OriginWin);

	// get the codepage id of the origin doc
	// the assumption is that it's the same one used for environment variables
	// as well as text in the config file
	BSTR charSet;
	hr = originDoc->get_charset(&charSet);
	if(hr == S_OK) {
		context->Codepage = Application.GetCharsetCode(charSet);
		SysFreeString(charSet);
	}
}

void WapacheWindow::InitExecContext(WapacheExecContext *context, const char *domain, const char *charSet)
{
	ZeroMemory(context, sizeof(WapacheExecContext));

	// create a temporary pool for string operations
	apr_pool_create(&context->TemporaryPool, Application.Process->pool);

	context->Scheme = "http";
	context->Domain = domain;

	context->OriginWin = NULL;

	BSTR charSetW;
	charSetW = CStrToWideString(charSet, -1);
	context->Codepage = Application.GetCharsetCode(charSetW);
	SysFreeString(charSetW);
}


void WapacheWindow::ClearExecContext(WapacheExecContext *context)
{
	apr_pool_destroy(context->TemporaryPool);
}

void WapacheWindow::ExecMenuCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	// the item specific function will fill in the other fields in the context
	context->Pattern = item->Pattern;
	switch(item->Type) {
	case STD_MENU_ITEM:
		ExecStdCommand(context, item);
		break;
	case STD_ENV_MENU_ITEM:
		ExecStdEnvCommand(context, item);
		break;
	case URL_MENU_ITEM:
		ExecUrlCommand(context, item);
		break;
	case URL_ENV_MENU_ITEM:
		ExecUrlEnvCommand(context, item);
		break;
	case JS_MENU_ITEM:
		ExecJSCommand(context, item);
		break;
	case JS_ENV_MENU_ITEM:
		ExecJSEnvCommand(context, item);
		break;
	case DOM_MENU_ITEM:
		ExecDOMCommand(context, item);
		break;
	case DOM_ENV_MENU_ITEM:
		ExecDOMEnvCommand(context, item);
		break;
	}
}

#define DISPID_THIS (-613)

bool CallMenuScriptMethod(HMENU hMenu, int pos, int id, void *data) {
	UINT *pId = (UINT *) data;
	if(id == *pId) {
		MENUITEMINFO mii;
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_DATA;
		if(GetMenuItemInfo(hMenu, pos, TRUE, &mii)) {
			IDispatch *itemDisp = (IDispatch *) mii.dwItemData;
			if(itemDisp) {
				VARIANT method;
				VariantInit(&method);
				if(GetProperty(itemDisp, L"method", &method, VT_DISPATCH)) {
					DISPID dispId = DISPID_THIS;
					VARIANT arg;
					VariantInit(&arg);
					V_VT(&arg) = VT_DISPATCH;
					V_DISPATCH(&arg) = itemDisp;

					DISPPARAMS params;
					params.cArgs = 1;
					params.cNamedArgs = 1;
					params.rgvarg = &arg;
					params.rgdispidNamedArgs = &dispId;

					HRESULT hr;
					IDispatch *methodDisp = V_DISPATCH(&method);
					hr = methodDisp->Invoke(DISPID_VALUE, IID_NULL, 0x0409, DISPATCH_METHOD, &params, NULL, NULL, NULL);

					VariantClear(&method);
				}
			}
		}
		return FALSE;
	}
	else {
		return TRUE;
	}
}

void WapacheWindow::ExecMenuCommand(WapacheExecContext *context, HMENU hMenu, UINT idm) 
{
	Application.LockApacheConfig(READ_LOCK);

	if(idm >= SCRIPT_MENU_ID_START) {
		EnumMenuRecursive(hMenu, CallMenuScriptMethod, &idm);		
	} else {
		// find the item
		wa_menu_item_config *item = Application.FindMenuItem(idm);
		if(item) {
			ExecMenuCommand(context, item);
		}
	}

	Application.UnlockApacheConfig(READ_LOCK);
}

struct FireMSHTMLCommandParams {
	const GUID *CommandGroup;
	DWORD CommandId;
};

void FireMSHTMLCommand(WapacheExecContext *context)
{
	HRESULT hr;
	FireMSHTMLCommandParams *p = reinterpret_cast<FireMSHTMLCommandParams *>(context->Data);
	IOleCommandTarget *target;
	hr = context->Browser->QueryInterface(&target);
	if(hr == S_OK) {
		if(!(context->Flags & TEST_DONT_EXEC)) {
			switch(p->CommandId) {
			case IDM_CLOSE:
				context->WinObj->Close(false);
				break;
			case IDM_FOCUS:
				context->WinObj->Focus();
				break;
			default:
				hr = target->Exec(p->CommandGroup, p->CommandId, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
				break;
			}
		}
		else {
			OLECMD cmd;
			switch(p->CommandId) {
			case IDM_CLOSE:
			case IDM_FOCUS:
				context->Result |= ITEM_AVAILABLE;
				break;
			default:
				cmd.cmdID = p->CommandId;
				cmd.cmdf = 0;
				hr = target->QueryStatus(p->CommandGroup, 1, &cmd, NULL);
				if(hr == S_OK) {
					if(cmd.cmdf & OLECMDF_ENABLED) {
						context->Result |= ITEM_AVAILABLE;
					}
				}
				break;
			}
		}
		target->Release();
	}
}

void WapacheWindow::ExecStdCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_std_menu_item_config *sItem = reinterpret_cast<wa_std_menu_item_config *>(item);

	if(ap_regexec(context->Pattern, "_app", 0, NULL, 0) == 0) {
		// an application command
		if(!(context->Flags & TEST_DONT_EXEC)) {
			switch(sItem->CommandId) {
			case IDM_EXIT:
				Application.Exit();
				break;
			}
		}
		else {
			switch(sItem->CommandId) {
			case IDM_EXIT:
				context->Result |= ITEM_AVAILABLE;
				break;
			}
		}
	}
	else {
		FireMSHTMLCommandParams p;
		p.CommandGroup = &CGID_MSHTML;
		p.CommandId = sItem->CommandId;

		context->Function = FireMSHTMLCommand;
		context->Data = &p;

		Broadcast(context);
	}
}

void WapacheWindow::ExecStdEnvCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_std_env_menu_item_config *sItem = reinterpret_cast<wa_std_env_menu_item_config *>(item);

	bool isSession, isPersistent;
	Application.GetEnvironmentVarType(context->Domain, sItem->EnvVarName, &isSession, &isPersistent);

	if(isSession) {
		Application.LockApacheEnv(WRITE_LOCK);
		if(!(context->Flags & TEST_DONT_EXEC)) {
			Application.SetEnvironmentVar(context->Domain, sItem->EnvVarName, sItem->EnvVarVal, isPersistent);
		}
		else {
			if(Application.CompareEnvironmentVar(context->Domain, sItem->EnvVarName, sItem->EnvVarVal)) {
				context->Result |= ENV_SET;
			}
		}
		Application.UnlockApacheEnv(WRITE_LOCK);
	}

	ExecStdCommand(context, sItem);
}

typedef BOOL (*ExecScriptMethodCallback)(void*, VARIANT*);

struct ExecScriptMethodParams {
	LPOLESTR Method;
	DISPPARAMS *Arguments;
	WORD Flags;
	ExecScriptMethodCallback Callback;
	void *CallbackData;
};

void InvokeScriptMethod(WapacheExecContext *context) 
{
	HRESULT hr;
	// get the scripting interface from IHTMLDocument
	IHTMLDocument *doc;
	IDispatch *script;
	hr = context->Doc->QueryInterface(&doc);
	if(hr == S_OK) {
		hr = doc->get_Script(&script);
		doc->Release();
	}
	if(hr == S_OK) {
		ExecScriptMethodParams *p = reinterpret_cast<ExecScriptMethodParams *>(context->Data); 
		DISPID dispId;
		hr = script->GetIDsOfNames(IID_NULL, &p->Method, 1, 0x0409, &dispId);
		if(hr == S_OK) {
			if(!(context->Flags & TEST_DONT_EXEC)) {
				VARIANT ret;
				VariantInit(&ret);
				hr = script->Invoke(dispId, IID_NULL, 0x0409, p->Flags, p->Arguments, &ret, NULL, NULL);
				if(p->Callback) {
					context->Aborted = !p->Callback(p->CallbackData, &ret);
				}
				VariantClear(&ret);
			}
			else {
				context->Result |= ITEM_AVAILABLE;
			}
		}
		script->Release();
	}
}

BOOL AbortOnFalse(void *, VARIANT *ret) {
	return (V_VT(ret) != VT_BOOL || V_BOOL(ret));
}

BOOL SaveResult(void *d, VARIANT *ret) {
	apr_array_header_t *t = (apr_array_header_t *) d;
	VARIANT *v = (VARIANT *) apr_array_push(t);
	VariantInit(v);
	VariantCopy(v, ret);
	return TRUE;
}

void WapacheWindow::ExecJSCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_js_menu_item_config *jItem = reinterpret_cast<wa_js_menu_item_config *>(item);
	apr_pool_t *pTemp = context->TemporaryPool;
	const char *domain = context->Domain;
	DWORD codepage = context->Codepage;

	// lock the environment first
	Application.LockApacheEnv(READ_LOCK);

	// expand the method name and convert it to Unicode
	const char *method = Application.ExpandEnvironmentStr(pTemp, jItem->Method, domain);
	LPOLESTR methodW = Application.ConvertString(codepage, method, -1);

	// expand the arguments and convert then to Unicode
	const char **argv;
	int argc;
	VARIANTARG *argvV;
	if(!(context->Flags & TEST_DONT_EXEC) && jItem->Args) {
		argv = reinterpret_cast<const char **>(jItem->Args->elts);
		argc = jItem->Args->nelts;
		argvV = new VARIANTARG[argc];
		for(int i = 0, j = argc - 1; i < argc; i++, j--) {
			const char *arg = Application.ExpandEnvironmentStr(pTemp, argv[i], domain);
			VariantInit(&argvV[j]);
			V_VT(&argvV[j]) = VT_BSTR;
			V_BSTR(&argvV[j]) = Application.ConvertString(codepage, arg, -1);
		}
	}
	else {
		argc = 0;
		argvV = NULL;
	}

	// unlock the environment now that all strings have been expanded
	Application.UnlockApacheEnv(READ_LOCK);

	// build the dispatch param structure
	DISPPARAMS dispParams;
	dispParams.cNamedArgs = 0;
	dispParams.rgdispidNamedArgs = NULL;
	dispParams.rgvarg = argvV;
	dispParams.cArgs = argc;

	// set the callback function and param
	ExecScriptMethodParams p;
	p.Method = methodW;
	p.Arguments = &dispParams;
	p.Flags = DISPATCH_METHOD;
	p.Callback = NULL;
	context->Function = InvokeScriptMethod;
	context->Data = &p;

	// broadcast the command to all windows
	Broadcast(context);

	// free the Unicode strings
	for(int i = 0; i < argc; i++) {
		VariantClear(&argvV[i]);
	}
	SysFreeString(methodW);
}

void WapacheWindow::ExecJSMethod(WapacheExecContext *context, const char *method, VARIANT *arg, int argCount, VARIANT **pResults, int *pResultCount) {
	// expand the method name and convert it to Unicode
	Application.LockApacheEnv(READ_LOCK);
	const char *methode = Application.ExpandEnvironmentStr(context->TemporaryPool, method, context->Domain);
	LPOLESTR methodW = Application.ConvertString(context->Codepage, methode, -1);
	Application.UnlockApacheEnv(READ_LOCK);

	DISPPARAMS dispParams;
	dispParams.cNamedArgs = 0;
	dispParams.rgdispidNamedArgs = NULL;
	dispParams.rgvarg = arg;
	dispParams.cArgs = argCount;

	// save results in an Apache array
	apr_array_header_t *results = apr_array_make(context->TemporaryPool, 4, sizeof(VARIANT));

	// set the callback function and param
	ExecScriptMethodParams p;
	p.Method = methodW;
	p.Arguments = &dispParams;
	p.Flags = DISPATCH_METHOD;
	p.Callback = SaveResult;
	p.CallbackData = results;
	context->Function = InvokeScriptMethod;
	context->Data = &p;

	// broadcast the command to all windows
	Broadcast(context);
	SysFreeString(methodW);

	*pResults = (VARIANT *) results->elts;
	*pResultCount = results->nelts;
}

bool WapacheWindow::BroadcastData(WapacheExecContext *context, LPOLESTR method, LPOLESTR data) {
	// place data into a variant
	VARIANT dataV;
	V_VT(&dataV) = VT_BSTR;
	V_BSTR(&dataV) = data;

	// build the dispatch param structure
	DISPPARAMS dispParams;
	dispParams.cNamedArgs = 0;
	dispParams.rgdispidNamedArgs = NULL;
	dispParams.rgvarg = &dataV;
	dispParams.cArgs = 1;

	VARIANT Continue;
	VariantInit(&Continue);

	ExecScriptMethodParams p;
	p.Method = method;
	p.Arguments = &dispParams;
	p.Flags = DISPATCH_METHOD;
	p.Callback = AbortOnFalse;
	p.CallbackData = NULL;
	context->Function = InvokeScriptMethod;
	context->Data = &p;

	// broadcast the command to all windows
	Broadcast(context);

	bool aborted = false;
	if(V_VT(&Continue) == VT_BOOL && !V_BOOL(&Continue)) {
		aborted = true;
	}
	return !aborted; 
}

void WapacheWindow::ExecJSEnvCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_js_env_menu_item_config *jItem = reinterpret_cast<wa_js_env_menu_item_config *>(item);
	bool isSession, isPersistent;
	Application.GetEnvironmentVarType(context->Domain, jItem->EnvVarName, &isSession, &isPersistent);

	if(isSession) {
		// set the environment variable
		Application.LockApacheEnv(WRITE_LOCK);
		if(!(context->Flags & TEST_DONT_EXEC)) {
			Application.SetEnvironmentVar(context->Domain, jItem->EnvVarName, jItem->EnvVarVal, isPersistent);
		}
		else {
			if(Application.CompareEnvironmentVar(context->Domain, jItem->EnvVarName, jItem->EnvVarVal)) {
				context->Result |= ENV_SET;
			}
		}
		Application.UnlockApacheEnv(WRITE_LOCK);
	}

	// let the regular function handle the rest
	ExecJSCommand(context, item);
}

struct InvokeDOMMethodParams {
	VARIANT Id;
	LPOLESTR Method;
	DISPPARAMS *Arguments;
	WORD Flags;
};

void InvokeDOMMethod(WapacheExecContext *context)
{
	HRESULT hr;
	InvokeDOMMethodParams *p = reinterpret_cast<InvokeDOMMethodParams *>(context->Data);
	IHTMLElementCollection *elems;
	IDispatch *elDisp = NULL;
	// get the "All" collection
	hr = context->Doc->get_all(&elems);
	if(hr == S_OK) {
		VARIANT index;
		VariantInit(&index);
		// get the dispatch interface of the element
		hr = elems->item(p->Id, index, &elDisp);
		elems->Release();
	}
	if(hr == S_OK && elDisp) {
		DISPID dispId;
		hr = elDisp->GetIDsOfNames(IID_NULL, &p->Method, 1, 0x0409, &dispId);
		if(hr == S_OK) {
			if(!(context->Flags & TEST_DONT_EXEC)) {
				hr = elDisp->Invoke(dispId, IID_NULL, 0x0409, p->Flags, p->Arguments, NULL, NULL, NULL);
			}
			else {
				context->Result |= ITEM_AVAILABLE;
			}
		}
		elDisp->Release();
	}
}

void WapacheWindow::ExecDOMCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_dom_menu_item_config *dItem = reinterpret_cast<wa_dom_menu_item_config *>(item);
	apr_pool_t *pTemp = context->TemporaryPool;
	const char *domain = context->Domain;
	DWORD codepage = context->Codepage;

	// expand all strings
	Application.LockApacheEnv(READ_LOCK);
	const char *id = Application.ExpandEnvironmentStr(pTemp, dItem->ElemId, domain);
	const char *prop = Application.ExpandEnvironmentStr(pTemp, dItem->PropName, domain);
	const char *value = Application.ExpandEnvironmentStr(pTemp, dItem->PropVal, domain);
	Application.UnlockApacheEnv(READ_LOCK);

	// convert them to Unicode
	LPOLESTR idW = Application.ConvertString(codepage, id, -1);
	LPOLESTR propW = Application.ConvertString(codepage, prop, -1);
	LPOLESTR valueW = Application.ConvertString(codepage, value, -1);

	// the id and value need to be variants
	VARIANT idV, valueV;
	VariantInit(&idV);
	VariantInit(&valueV);
	V_VT(&idV) = VT_BSTR;
	V_BSTR(&idV) = idW;
	V_VT(&valueV) = VT_BSTR;
	V_BSTR(&valueV) = valueW;

	DISPPARAMS dispParams;
	dispParams.cNamedArgs = 0;
	dispParams.rgdispidNamedArgs = NULL;
	dispParams.rgvarg = &valueV;
	dispParams.cArgs = 1;

	// set the callback
	InvokeDOMMethodParams p;
	p.Id = idV;
	p.Method = propW;
	p.Arguments = &dispParams;
	p.Flags = DISPATCH_PROPERTYPUT;
	context->Function = InvokeDOMMethod;
	context->Data = &p;

	Broadcast(context);

	VariantClear(&idV);
	SysFreeString(propW);
	VariantClear(&valueV);
}

void WapacheWindow::ExecDOMEnvCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_dom_env_menu_item_config *dItem = reinterpret_cast<wa_dom_env_menu_item_config *>(item);

	bool isSession, isPersistent;
	Application.GetEnvironmentVarType(context->Domain, dItem->EnvVarName, &isSession, &isPersistent);

	if(isSession) {
		Application.LockApacheEnv(WRITE_LOCK);
		if(!(context->Flags & TEST_DONT_EXEC)) {
			Application.SetEnvironmentVar(context->Domain, dItem->EnvVarName, dItem->EnvVarVal, isPersistent);
		}
		else {
			if(Application.CompareEnvironmentVar(context->Domain, dItem->EnvVarName, dItem->EnvVarVal)) {
				context->Result |= ENV_SET;
			}
		}
		Application.UnlockApacheEnv(WRITE_LOCK);
	}

	ExecDOMCommand(context, item);
}

struct NavigateToUrlParams {
	LPOLESTR Url;
	LPOLESTR Target;
};

void NavigateToUrl(WapacheExecContext *context) 
{
	NavigateToUrlParams *p = reinterpret_cast<NavigateToUrlParams *>(context->Data);
	if(!(context->Flags & TEST_DONT_EXEC)) {
		VARIANT targetV;
		VariantInit(&targetV);
		V_VT(&targetV) = VT_BSTR;
		V_BSTR(&targetV) = p->Target;
		context->Browser->Navigate(p->Url, NULL, &targetV, NULL, NULL);
	}
	else {
		context->Result |= ITEM_AVAILABLE;
	}
}

void WapacheWindow::ExecUrlCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_url_menu_item_config *uItem = reinterpret_cast<wa_url_menu_item_config *>(item);
	apr_pool_t *pTemp = context->TemporaryPool;
	const char *domain = context->Domain;
	DWORD codepage = context->Codepage;
	const char *url;
	const char *target;
	LPOLESTR urlW = NULL;
	LPOLESTR targetW = NULL;

	// expand all strings
	if(!(context->Flags & TEST_DONT_EXEC)) {
		Application.LockApacheEnv(READ_LOCK);
		url = Application.ExpandEnvironmentStr(pTemp, uItem->Url, domain);
		target = Application.ExpandEnvironmentStr(pTemp, uItem->Target, domain);
		urlW = Application.ConvertString(codepage, url, -1);
		targetW = Application.ConvertString(codepage, target, -1);
		Application.UnlockApacheEnv(READ_LOCK);
	}

	if(ap_regexec(context->Pattern, "_app", 0, NULL, 0) == 0) {
		// an application command
		if(!(context->Flags & TEST_DONT_EXEC)) {
			if(!target) target = "";
			WapacheWindow *win;
			if(OpenUrl("_app", url, target, &win)) {
				win->Release();
			}
		}
		else {
			context->Result |= ITEM_AVAILABLE;
		}
	}
	else {
		if(!uItem->Target) {
			context->Flags |= DONT_CHECK_SCHEME | DONT_CHECK_DOMAIN;
		}

		// set callback
		NavigateToUrlParams p;
		p.Target = targetW;
		p.Url = urlW;
		context->Function = NavigateToUrl;
		context->Data = &p;

		Broadcast(context);
	}

	SysFreeString(urlW);
	SysFreeString(targetW);
}

void WapacheWindow::ExecUrlEnvCommand(WapacheExecContext *context, wa_menu_item_config *item)
{
	wa_url_env_menu_item_config *uItem = reinterpret_cast<wa_url_env_menu_item_config *>(item);

	bool isSession, isPersistent;
	Application.GetEnvironmentVarType(context->Domain, uItem->EnvVarName, &isSession, &isPersistent);

	if(isSession) {
		Application.LockApacheEnv(WRITE_LOCK);
		if(!(context->Flags & TEST_DONT_EXEC)) {
			Application.SetEnvironmentVar(context->Domain, uItem->EnvVarName, uItem->EnvVarVal, isPersistent);
		}
		else {
			if(Application.CompareEnvironmentVar(context->Domain, uItem->EnvVarName, uItem->EnvVarVal)) {
				context->Result |= ENV_SET;
			}
		}
		Application.UnlockApacheEnv(WRITE_LOCK);
	}

	ExecUrlCommand(context, item);
}

bool WapacheWindow::CreateScriptObject(VARIANT *p) {
	HRESULT hr;
	IHTMLDocument2 *doc;
	IDispatch *docDisp;
	IDispatch *script;
	hr = Browser->get_Document(&docDisp);
	if(hr == S_OK) {
		hr = docDisp->QueryInterface(&doc);
		docDisp->Release();
	}
	if(hr == S_OK) {
		hr = doc->get_Script(&script);
		doc->Release();
	}
	if(hr == S_OK) {
		// get the class object first
		DISPID dispId;
		LPWSTR className = L"Object";
		DISPPARAMS params;
		VARIANT ret;

		params.cArgs = 0;
		params.cNamedArgs = 0;
		VariantInit(&ret);
		hr = script->GetIDsOfNames(IID_NULL, &className, 1, 0x0409, &dispId);
		if(hr == S_OK) {
			hr = script->Invoke(dispId, IID_NULL, 0x0409, DISPATCH_PROPERTYGET, &params, &ret, NULL, NULL);
		}
		if(hr == S_OK && V_VT(&ret) == VT_DISPATCH) {
			IDispatch *classDisp = V_DISPATCH(&ret);
			params.cArgs = 0;
			params.cNamedArgs = 0;
			hr = classDisp->Invoke(DISPID_VALUE, IID_NULL, 0x0409, DISPATCH_METHOD, &params, p, NULL, NULL);
		}
		VariantClear(&ret);
	}
	return (hr == S_OK);
}

HRESULT STDMETHODCALLTYPE WapacheWindow::ShowContextMenu( 
    /* [in] */ DWORD dwID,
    /* [in] */ POINT *ppt,
    /* [in] */ IUnknown *pcmdtReserved,
    /* [in] */ IDispatch *pdispReserved)
{
	HRESULT hr = S_FALSE;

	IServiceProvider *serf;
	IInternetHostSecurityManager *iman;
	pdispReserved->QueryInterface(&serf);
	serf->QueryService(SID_SInternetHostSecurityManager, &iman);
	BYTE secId[512];
	DWORD secIdSize = 512;
	iman->GetSecurityId(secId, &secIdSize, NULL);
	iman->Release();
	serf->Release();

	HMENU hMenu = NULL;
	Application.LockApacheConfig(READ_LOCK);
	wa_win_config *winSettings = Application.GetWindowSettings(Name);

	if(winSettings) {
		const char *menuName = NULL;
		switch(dwID) {
			case CONTEXT_MENU_DEFAULT: menuName = winSettings->DefaultMenu; break;
			case CONTEXT_MENU_IMAGE: menuName = winSettings->ImageMenu; break;
			case CONTEXT_MENU_CONTROL: menuName = winSettings->ControlMenu; break; 
			case CONTEXT_MENU_TABLE: menuName = winSettings->TableMenu; break; 
			case CONTEXT_MENU_TEXTSELECT: menuName = winSettings->TextSelectMenu; break; 
			case CONTEXT_MENU_ANCHOR: menuName = winSettings->AnchorMenu; break; 
		}
		if(menuName) {
			hMenu = Application.BuildPopupMenu(menuName, pdispReserved);
			hr = S_OK;
		}
	}
	Application.UnlockApacheConfig(READ_LOCK);

	if(hMenu) {
		IHTMLDocument2 *doc;
		hr = pcmdtReserved->QueryInterface(&doc);

		if(hr == S_OK) {
			WapacheExecContext e;
			InitExecContext(&e, doc);
			SetMenuItemAvailability(&e, hMenu);
			int cmd = TrackPopupMenu(hMenu, TPM_NONOTIFY | TPM_RETURNCMD, ppt->x, ppt->y, 0, 
									 Hwnd, NULL);
			if(cmd) {
				ExecMenuCommand(&e, hMenu, cmd);
			}
			ClearExecContext(&e);
			doc->Release();
		}
		Application.DestroyMenu(hMenu);
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetHostInfo( 
    /* [out][in] */ DOCHOSTUIINFO *pInfo)
{
	pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
	pInfo->dwFlags = HostUIFlags;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::ShowUI( 
    /* [in] */ DWORD dwID,
    /* [in] */ IOleInPlaceActiveObject *pActiveObject,
    /* [in] */ IOleCommandTarget *pCommandTarget,
    /* [in] */ IOleInPlaceFrame *pFrame,
    /* [in] */ IOleInPlaceUIWindow *pDoc)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::HideUI( void)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE WapacheWindow::UpdateUI( void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnDocWindowActivate( 
    /* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::OnFrameWindowActivate( 
    /* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::ResizeBorder( 
    /* [in] */ LPCRECT prcBorder,
    /* [in] */ IOleInPlaceUIWindow *pUIWindow,
    /* [in] */ BOOL fRameWindow)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::TranslateAccelerator( 
    /* [in] */ LPMSG lpMsg,
    /* [in] */ const GUID *pguidCmdGroup,
    /* [in] */ DWORD nCmdID)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetOptionKeyPath( 
    /* [out] */ LPOLESTR *pchKey,
    /* [in] */ DWORD dw)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetDropTarget( 
    /* [in] */ IDropTarget *pDropTarget,
    /* [out] */ IDropTarget **ppDropTarget)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetExternal( 
    /* [out] */ IDispatch **ppDispatch)
{
	External->AddRef();
	Application.LockApacheConfig(READ_LOCK);
	BOOL externalAvailable = Application.ClientConf->external;
	Application.UnlockApacheConfig(READ_LOCK);
	if(externalAvailable) {
		*ppDispatch = External;
		return S_OK;
	}
	else {
		*ppDispatch = NULL;
		return S_FALSE;
	}
}

HRESULT STDMETHODCALLTYPE WapacheWindow::TranslateUrl( 
    /* [in] */ DWORD dwTranslate,
    /* [in] */ OLECHAR *pchURLIn,
    /* [out] */ OLECHAR **ppchURLOut)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::FilterDataObject( 
    /* [in] */ IDataObject *pDO,
    /* [out] */ IDataObject **ppDORet)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetOverrideKeyPath( 
    /* [out] */ LPOLESTR *pchKey,
    /* [in] */ DWORD dw)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetSecuritySite( 
    /* [unique][in] */ IInternetSecurityMgrSite *pSite)
{
	return INET_E_DEFAULT_ACTION;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetSecuritySite( 
    /* [out] */ IInternetSecurityMgrSite **ppSite)
{
	return INET_E_DEFAULT_ACTION;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::MapUrlToZone( 
    /* [in] */ LPCWSTR pwszUrl,
    /* [out] */ DWORD *pdwZone,
    /* [in] */ DWORD dwFlags)
{
	return INET_E_DEFAULT_ACTION;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetSecurityId( 
    /* [in] */ LPCWSTR pwszUrl,
    /* [size_is][out] */ BYTE *pbSecurityId,
    /* [out][in] */ DWORD *pcbSecurityId,
    /* [in] */ DWORD_PTR dwReserved)
{
	return INET_E_DEFAULT_ACTION;
}

#define URLACTION_SHELL_POPUPMGR		0x00001809

HRESULT STDMETHODCALLTYPE WapacheWindow::ProcessUrlAction( 
    /* [in] */ LPCWSTR pwszUrl,
    /* [in] */ DWORD dwAction,
    /* [size_is][out] */ BYTE *pPolicy,
    /* [in] */ DWORD cbPolicy,
    /* [in] */ BYTE *pContext,
    /* [in] */ DWORD cbContext,
    /* [in] */ DWORD dwFlags,
    /* [in] */ DWORD dwReserved)
{
	DWORD *p = (DWORD *) pPolicy;
	switch(dwAction) {
		case URLACTION_COOKIES_ENABLED:
		case URLACTION_COOKIES_SESSION:
		case URLACTION_HTML_SUBMIT_FORMS:
			*p = URLPOLICY_ALLOW;
			break;
		case URLACTION_SCRIPT_SAFE_ACTIVEX:
		case URLACTION_SCRIPT_JAVA_USE:
		case URLACTION_SCRIPT_RUN:
			*p = (Application.ClientConf->Javascript) ? URLPOLICY_ALLOW : URLPOLICY_DISALLOW;
			break;
		case URLACTION_ACTIVEX_RUN:
			*p = (Application.ClientConf->ActiveX) ? URLPOLICY_ALLOW : URLPOLICY_DISALLOW;
			break;
		case URLACTION_HTML_JAVA_RUN:
			*p = (Application.ClientConf->Java) ? URLPOLICY_ALLOW : URLPOLICY_DISALLOW;
			break;
		case URLACTION_JAVA_PERMISSIONS:
			*p = URLPOLICY_JAVA_LOW;
			break;
		case URLACTION_SHELL_POPUPMGR:
			*p = URLPOLICY_ALLOW;
			break;
		default:
			return INET_E_DEFAULT_ACTION;
	}
	return (*p == URLPOLICY_ALLOW) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::QueryCustomPolicy( 
    /* [in] */ LPCWSTR pwszUrl,
    /* [in] */ REFGUID guidKey,
    /* [size_is][size_is][out] */ BYTE **ppPolicy,
    /* [out] */ DWORD *pcbPolicy,
    /* [in] */ BYTE *pContext,
    /* [in] */ DWORD cbContext,
    /* [in] */ DWORD dwReserved)
{
	return INET_E_DEFAULT_ACTION;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::SetZoneMapping( 
    /* [in] */ DWORD dwZone,
    /* [in] */ LPCWSTR lpszPattern,
    /* [in] */ DWORD dwFlags)
{
	return INET_E_DEFAULT_ACTION;
}

HRESULT STDMETHODCALLTYPE WapacheWindow::GetZoneMappings( 
    /* [in] */ DWORD dwZone,
    /* [out] */ IEnumString **ppenumString,
    /* [in] */ DWORD dwFlags)
{
	return INET_E_DEFAULT_ACTION;
}
