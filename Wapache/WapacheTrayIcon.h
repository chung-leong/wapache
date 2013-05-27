#ifndef __WAPACHETRAYICON_H
#define __WAPACHETRAYICON_H

class WapacheTrayIcon {

public:
	static void OnApplicationStart(void);
	static void OnApplicationEnd(void);
	static void OnApacheConfigChange(void);

	WapacheTrayIcon(wa_sticon_config *settings);
	~WapacheTrayIcon(void);
	bool Start(void);

	ULONG AddRef(void);
	ULONG Release(void);

private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnCreate(CREATESTRUCT *cs);
	void OnDestroy(void);
	void OnContextMenu(UINT uMsg, int x, int y);
	void OnMouseMove(int x, int y);
	
private:
	WapacheTrayIcon *Prev;
	WapacheTrayIcon *Next;

	char *IconPath;
	char *ToolTip;
	char *CharSet;
	char *Domain;
	bool AutoExit;
	char *LeftClickMenu;
	int LeftClickItemIndex;
	char *RightClickMenu;
	int RightClickItemIndex;
	char *DoubleClickMenu;
	int DoubleClickItemIndex;

	ULONG RefCount;
	HWND Hwnd;
	HICON HIcon;

	static WapacheTrayIcon *IconList;
	static bool ClassRegistered;
	static ATOM ClassAtom;
};

#endif