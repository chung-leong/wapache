// main.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "WapacheApplication.h"

HINSTANCE _hinstance;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	_hinstance = hInstance;
	return Application.Run();
}
