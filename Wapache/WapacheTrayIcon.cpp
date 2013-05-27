#include "StdAfx.h"
#include "WapacheTrayIcon.h"
#include "WapacheApplication.h"
#include "WapacheWindow.h"

WapacheTrayIcon *WapacheTrayIcon::IconList;
bool WapacheTrayIcon::ClassRegistered;
ATOM WapacheTrayIcon::ClassAtom;

void WapacheTrayIcon::OnApplicationStart(void)
{
	wa_sticon_config **icons = (wa_sticon_config **) Application.ClientConf->sec_sticon->elts;
	int count = Application.ClientConf->sec_sticon->nelts;
	for(int i = 0; i < count; i++) {
		WapacheTrayIcon *icon = new WapacheTrayIcon(icons[i]);
		icon->Start();
	}
}

void WapacheTrayIcon::OnApplicationEnd(void)
{
	if(IconList) {
		WapacheTrayIcon *icon = IconList;
        
		do {
			SendMessage(icon->Hwnd, WM_CLOSE, 0, 0);
			icon = icon->Next;
		} while(icon != IconList);
	}
}

void WapacheTrayIcon::OnApacheConfigChange(void)
{
	// just recreate the icons
	OnApplicationEnd();
	OnApplicationStart();
}

WapacheTrayIcon::WapacheTrayIcon(wa_sticon_config *settings)
{
	Prev = NULL;
	Next = NULL;
	HIcon = NULL;

	IconPath = (settings->IconPath) ? strdup(settings->IconPath) : NULL;
	ToolTip = (settings->ToolTip) ? strdup(settings->ToolTip) : NULL;
	CharSet = strdup(settings->CharSet ? settings->CharSet : "iso-8859-1");
	Domain = (settings->Domain) ? strdup(settings->Domain) : NULL;
	AutoExit = settings->AutoExit;
	LeftClickMenu = (settings->LeftClickMenu) ? strdup(settings->LeftClickMenu) : NULL;
	LeftClickItemIndex = settings->LeftClickItemIndex;
	RightClickMenu = (settings->RightClickMenu) ? strdup(settings->RightClickMenu) : NULL;
	RightClickItemIndex = settings->RightClickItemIndex;
	DoubleClickMenu = (settings->DoubleClickMenu) ? strdup(settings->DoubleClickMenu) : NULL;
	DoubleClickItemIndex = settings->DoubleClickItemIndex;
}

WapacheTrayIcon::~WapacheTrayIcon(void)
{
	delete[] IconPath;
	delete[] ToolTip;
	delete[] CharSet;
	delete[] Domain;
	delete[] LeftClickMenu;
	delete[] RightClickMenu;
	delete[] DoubleClickMenu;

	DestroyIcon(HIcon);
}

bool WapacheTrayIcon::Start(void)
{
	if(!ClassRegistered) {
		WNDCLASSEX wc;

		wc.cbSize = sizeof(wc);
		wc.style = 0;
		wc.lpfnWndProc = WindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = _hinstance;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = "WapacheTrayIcon";
		wc.hIconSm = NULL;

		ClassAtom = RegisterClassEx(&wc);
		ClassRegistered = true;
	}


	CreateWindowEx(0, "WapacheTrayIcon", "", 0, 0, 0, 0, 0, NULL, NULL, _hinstance, reinterpret_cast<LPVOID>(this));
	if(!Hwnd) {
		return false;
	}

	return true;
}

ULONG WapacheTrayIcon::AddRef(void)
{
	return ++RefCount;
}

ULONG WapacheTrayIcon::Release(void)
{
	if(--RefCount == 0) {
		delete this;
		return 0;
	}
	return RefCount;
}

#define WM_ICON_NOTIFY	WM_USER + 10

LRESULT CALLBACK WapacheTrayIcon::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WapacheTrayIcon *icon;

	if(uMsg == WM_CREATE) {
		CREATESTRUCT *c = reinterpret_cast<CREATESTRUCT *>(lParam);

		SetWindowLong(hwnd, GWL_USERDATA, reinterpret_cast<LONG>(c->lpCreateParams));
		icon = reinterpret_cast<WapacheTrayIcon *>(c->lpCreateParams);
		icon->Hwnd = hwnd;
		icon->AddRef();
		icon->OnCreate(c);
		// add a reference to the application object 
		// if we don't wish to exit automatically
		if(!icon->AutoExit) {
			Application.AddRef();
		}
	}
	else {
		icon = reinterpret_cast<WapacheTrayIcon *>(GetWindowLong(hwnd, GWL_USERDATA));
		if(uMsg == WM_DESTROY) {
			icon->OnDestroy();
			icon->Hwnd = NULL;
			icon->Release();
			if(!icon->AutoExit) {
				Application.Release();
			}
		}
		else if(uMsg == WM_ICON_NOTIFY) {
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			
			switch(lParam) {
				case WM_RBUTTONUP:
				case WM_LBUTTONUP:
				case WM_LBUTTONDBLCLK:
					icon->OnContextMenu(lParam, cursorPos.x, cursorPos.y);
					break;
				case WM_MOUSEMOVE:
					icon->OnMouseMove(cursorPos.x, cursorPos.y);
					break;
			}
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void WapacheTrayIcon::OnCreate(CREATESTRUCT *cs)
{
	NOTIFYICONDATA niData;

	// load the icon
	const char *iconPath = IconPath;
	if(!iconPath) {
		iconPath = Application.ClientConf->def_icon_path;
	}

	if(iconPath) {
		int smIconX = GetSystemMetrics(SM_CXSMICON);
		int smIconY = GetSystemMetrics(SM_CYSMICON);
		HIcon = reinterpret_cast<HICON>(LoadImage(0, iconPath, IMAGE_ICON, 
														smIconX, smIconY, LR_LOADFROMFILE));
	}

	ZeroMemory(&niData, sizeof(niData));
	niData.cbSize = sizeof(niData);
	niData.hWnd = Hwnd;
	niData.uFlags = NIF_ICON | NIF_MESSAGE;
	niData.hIcon = HIcon;
	niData.uCallbackMessage = WM_ICON_NOTIFY;

	Shell_NotifyIcon(NIM_ADD, &niData);

	if(!IconList) {
		IconList = this;
		this->AddRef();
		IconList->Prev =  this;
		this->AddRef();
		IconList->Next = this;
	}
	else {
		Prev = IconList->Prev;
		this->AddRef();
		IconList->Prev->Next = this;
		Next = IconList;
		this->AddRef();
		IconList->Prev = this;
	}
}

void WapacheTrayIcon::OnDestroy(void)
{
	NOTIFYICONDATA niData;

	ZeroMemory(&niData, sizeof(niData));
	niData.cbSize = sizeof(niData);
	niData.hWnd = Hwnd;

	Shell_NotifyIcon(NIM_DELETE, &niData);

	Prev->Next = Next;
	this->Release();
	Next->Prev = Prev;
	this->Release();
	Next = Prev = NULL;
	if(IconList == this) {
		IconList = NULL;
	}
}

void WapacheTrayIcon::OnContextMenu(UINT uMsg, int x, int y)
{
	const char *menuName = NULL;
	int menuIndex = 0;
	HMENU hMenu = NULL;

	switch(uMsg) {
		case WM_RBUTTONUP: 
			menuName = RightClickMenu;
			menuIndex = RightClickItemIndex;
			break;
		case WM_LBUTTONUP:
			menuName = LeftClickMenu;
			menuIndex = LeftClickItemIndex;
			break;
		case WM_LBUTTONDBLCLK:
			menuName = DoubleClickMenu;
			menuIndex = DoubleClickItemIndex;
			break;
	}

	if(menuName) {
		const char *domain = (Domain) ? Domain : Application.ServerConf->server_hostname;
		hMenu = Application.BuildPopupMenu(menuName);
		WapacheExecContext e;
		WapacheWindow::InitExecContext(&e, domain, CharSet);
		WapacheWindow::SetMenuItemAvailability(&e, hMenu);
		WapacheWindow::ClearExecContext(&e);

		int cmd = 0;
		
		if(!menuIndex) {
			SetForegroundWindow(Hwnd);
			cmd = TrackPopupMenu(hMenu, TPM_NONOTIFY | TPM_RETURNCMD, x, y, 0, Hwnd, NULL);
			PostMessage(Hwnd, WM_NULL, 0, 0);
		}
		else {
			cmd = GetMenuItemID(hMenu, menuIndex - 1);
		}

		if(cmd) {
			WapacheWindow::InitExecContext(&e, domain, CharSet);
			WapacheWindow::ExecMenuCommand(&e, hMenu, cmd);
			WapacheWindow::ClearExecContext(&e);
		}

		Application.DestroyMenu(hMenu);
	}
}

void WapacheTrayIcon::OnMouseMove(int x, int y) {
	if(ToolTip) {
		apr_pool_t *pTemp;
		apr_pool_create(&pTemp, Application.Process->pool);
		const char *tooltip = Application.ExpandEnvironmentStr(pTemp, ToolTip, Domain);

		NOTIFYICONDATA niData;

		ZeroMemory(&niData, sizeof(niData));
		niData.cbSize = sizeof(niData);
		strncpy(niData.szTip, tooltip, 63);
		niData.uFlags |= NIF_TIP;
		niData.hWnd = Hwnd;

		Shell_NotifyIcon(NIM_MODIFY, &niData);

		apr_pool_destroy(pTemp);
	}
}
