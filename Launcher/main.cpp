#define WIN32_LEAD_AND_MEAN 
#include <windows.h>
#include <shlwapi.h>

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	char base_path[MAX_PATH];
	char bin_path[MAX_PATH];
	char exe_path[MAX_PATH];
	GetModuleFileName(hInstance, base_path, sizeof(bin_path));
	PathRemoveFileSpec(base_path);

	PathCombine(bin_path, base_path, "bin");
	PathCombine(exe_path, base_path, "bin\\wapache.exe");

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE wapache_handle;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	CreateProcess(exe_path, NULL, NULL, NULL, TRUE, 0, NULL, bin_path, &si, &pi);
	return 0;
}
