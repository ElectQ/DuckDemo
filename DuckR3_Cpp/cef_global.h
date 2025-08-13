#pragma once
#include "main_app.h"
#include <map>
#include <mutex>

class SuperDuckAntiVirusCefApp;

// 弹窗管理结构
struct PopupInfo {
    HWND windowHandle;
    CefRefPtr<CefBrowser> browserHandle;
    int eventId;
    
    PopupInfo() : windowHandle(nullptr), browserHandle(nullptr), eventId(-1) {}
    PopupInfo(HWND wnd, CefRefPtr<CefBrowser> browser, int id) 
        : windowHandle(wnd), browserHandle(browser), eventId(id) {}
};

namespace cef_global {
    // 主窗口相关
    extern HWND cefWindowHandle;
    extern WNDPROC cefBrowserWindowProc;
    extern CefRefPtr<CefBrowser> cefBrowserHandle;
    extern CefRefPtr<SuperDuckAntiVirusCefApp> MainCefApp;
    
    // 弹窗管理 - 支持多个弹窗
    extern std::map<int, PopupInfo> activePopups;  // eventId -> PopupInfo
    extern std::mutex popupMapMutex;
    extern const int MAX_CONCURRENT_POPUPS;
    
    // 弹窗管理函数
    auto AddPopup(int eventId, HWND windowHandle, CefRefPtr<CefBrowser> browserHandle) -> void;
    auto RemovePopup(int eventId) -> void;
    auto GetPopupByEventId(int eventId) -> PopupInfo*;
    auto GetPopupByBrowserId(int browserId) -> PopupInfo*;
    auto GetActivePopupCount() -> int;
    
    // 异步清理机制
    auto AsyncCleanupPopup(int eventId) -> void;
    auto ScheduleWindowDestroy(HWND windowHandle, int eventId) -> void;

    // 保留旧的全局变量用于兼容性（可能被一些地方引用）
    extern HWND cefEventWindowHandle;  // 废弃，但保留避免编译错误
    extern WNDPROC cefBrowserEventWindowProc;  // 废弃，但保留避免编译错误
    extern CefRefPtr<CefBrowser> cefBrowserEventHandle;  // 废弃，但保留避免编译错误
};