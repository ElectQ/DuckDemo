#include "cef_global.h"
namespace cef_global {
	// 全局变量
	HWND cefWindowHandle;
	HWND cefEventWindowHandle;

	WNDPROC cefBrowserWindowProc;
	WNDPROC cefBrowserEventWindowProc;

	CefRefPtr<CefBrowser> cefBrowserHandle;
	CefRefPtr<CefBrowser> cefBrowserEventHandle;
	CefRefPtr<SuperDuckAntiVirusCefApp> MainCefApp = nullptr;
};