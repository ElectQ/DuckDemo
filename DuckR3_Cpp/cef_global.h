#pragma once
#include "main_app.h"

class SuperDuckAntiVirusCefApp;

namespace cef_global {
    // 全局变量
    extern HWND cefWindowHandle;
    extern HWND cefEventWindowHandle;
    extern WNDPROC cefBrowserWindowProc;
    extern WNDPROC cefBrowserEventWindowProc;
    extern CefRefPtr<CefBrowser> cefBrowserHandle;
    extern CefRefPtr<CefBrowser> cefBrowserEventHandle;
    extern CefRefPtr<SuperDuckAntiVirusCefApp> MainCefApp;

};