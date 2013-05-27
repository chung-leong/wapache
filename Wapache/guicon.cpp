#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

#define ENABLE_INSERT_MODE		0x0020
#define ENABLE_QUICK_EDIT_MODE	0x0040
#define ENABLE_EXTENDED_FLAGS	0x0080
#define ENABLE_AUTO_POSITION	0x0100

// maximum mumber of lines the output console should have

static const WORD MAX_CONSOLE_LINES = 500;

typedef HWND (WINAPI *GetConsoleWindowProc)(void);

LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_CLOSE:
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL redirecting;

BOOL RedirectingToConsole(void) {
	return redirecting;
}

void RedirectStreamsToStardardHandlers(void) {
	FILE *fp;
	long hConHandle;
	long lStdHandle;

	// redirect unbuffered STDOUT to the console
	lStdHandle = (long) GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
	setvbuf( stdout, NULL, _IONBF, 0 );
	// redirect unbuffered STDIN to the console
	lStdHandle = (long) GetStdHandle(STD_INPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "r" );
	*stdin = *fp;
	setvbuf( stdin, NULL, _IONBF, 0 );

	// redirect unbuffered STDERR to the console
	lStdHandle = (long) GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stderr = *fp;
	setvbuf( stderr, NULL, _IONBF, 0 );
}

void RedirectIOToConsole(BOOL state) {
	redirecting = state;
	if(state) {
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		SMALL_RECT rect;

		// allocate a console for this app
		if(!AllocConsole()) {
			return;
		}

		HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
		HANDLE input  = GetStdHandle(STD_INPUT_HANDLE);

		// set the screen buffer to be big enough to let us scroll text
		GetConsoleScreenBufferInfo(output, &coninfo);
		coninfo.dwSize.Y = MAX_CONSOLE_LINES;
		SetConsoleScreenBufferSize(output, coninfo.dwSize);

		// enable quick edit
		DWORD mode;
		GetConsoleMode(input, &mode);
		SetConsoleMode(input, mode | ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE);

		// GetConsoleWindow is Win2K only--find it dynamically
		HMODULE lib = LoadLibrary("kernel32.dll");
		GetConsoleWindowProc GetConsoleWindow = (GetConsoleWindowProc) GetProcAddress(lib, "GetConsoleWindow");
		if(GetConsoleWindow) {
			HWND hwnd = GetConsoleWindow();

			// prevent closing of the console window
			HMENU menu = GetSystemMenu(hwnd, FALSE); 
			DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND); 
			DrawMenuBar(hwnd); 
		}
		FreeLibrary(lib);
		RedirectStreamsToStardardHandlers();
	} else {
		FreeConsole();
	}
}


//End of File
