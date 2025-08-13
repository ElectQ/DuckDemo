#include "cef_global.h"
namespace cef_global {
	// 主窗口相关
	HWND cefWindowHandle;
	WNDPROC cefBrowserWindowProc;
	CefRefPtr<CefBrowser> cefBrowserHandle;
	CefRefPtr<SuperDuckAntiVirusCefApp> MainCefApp = nullptr;
	
	// 弹窗管理 - 支持多个弹窗
	std::map<int, PopupInfo> activePopups;
	std::mutex popupMapMutex;
	const int MAX_CONCURRENT_POPUPS = 1;
	
	// 保留旧的全局变量用于兼容性
	HWND cefEventWindowHandle = nullptr;
	WNDPROC cefBrowserEventWindowProc = nullptr;
	CefRefPtr<CefBrowser> cefBrowserEventHandle = nullptr;
	
	// 弹窗管理函数实现
	auto AddPopup(int eventId, HWND windowHandle, CefRefPtr<CefBrowser> browserHandle) -> void {
		std::lock_guard<std::mutex> lock(popupMapMutex);
		activePopups[eventId] = PopupInfo(windowHandle, browserHandle, eventId);
		printf("[PopupManager] Added popup: eventId=%d, windowHandle=%p, browserID=%d\n", 
			eventId, windowHandle, browserHandle ? browserHandle->GetIdentifier() : -1);
		printf("[PopupManager] Active popups count: %d\n", (int)activePopups.size());
	}
	
	auto RemovePopup(int eventId) -> void {
		std::lock_guard<std::mutex> lock(popupMapMutex);
		auto it = activePopups.find(eventId);
		if (it != activePopups.end()) {
			printf("[PopupManager] Removing popup: eventId=%d\n", eventId);
			activePopups.erase(it);
			printf("[PopupManager] Active popups count: %d\n", (int)activePopups.size());
		} else {
			printf("[PopupManager] Warning: Tried to remove non-existent popup: eventId=%d\n", eventId);
		}
	}
	
	auto GetPopupByEventId(int eventId) -> PopupInfo* {
		std::lock_guard<std::mutex> lock(popupMapMutex);
		auto it = activePopups.find(eventId);
		return (it != activePopups.end()) ? &it->second : nullptr;
	}
	
	auto GetPopupByBrowserId(int browserId) -> PopupInfo* {
		std::lock_guard<std::mutex> lock(popupMapMutex);
		for (auto& pair : activePopups) {
			if (pair.second.browserHandle && 
				pair.second.browserHandle->GetIdentifier() == browserId) {
				return &pair.second;
			}
		}
		printf("[PopupManager] No popup found for browserID=%d\n", browserId);
		return nullptr;
	}
	
	auto GetActivePopupCount() -> int {
		std::lock_guard<std::mutex> lock(popupMapMutex);
		return (int)activePopups.size();
	}
	
	// 异步清理机制 - 防止卡顿
	auto AsyncCleanupPopup(int eventId) -> void {
		printf("[PopupManager] AsyncCleanupPopup called for eventId=%d\n", eventId);
		
		// 在新线程中异步清理，避免阻塞UI线程
		std::thread([eventId]() {
			Sleep(100);  // 给浏览器一点时间完成关闭
			
			printf("[PopupManager] AsyncCleanupPopup thread started for eventId=%d\n", eventId);
			
			std::lock_guard<std::mutex> lock(popupMapMutex);
			auto it = activePopups.find(eventId);
			if (it != activePopups.end()) {
				HWND windowHandle = it->second.windowHandle;
				printf("[PopupManager] Found popup to cleanup: eventId=%d, windowHandle=%p\n", eventId, windowHandle);
				
				// 在销毁前检查窗口状态
				if (windowHandle) {
					BOOL isValidWindow = IsWindow(windowHandle);
					BOOL isVisible = IsWindowVisible(windowHandle);
					DWORD windowPid = 0;
					GetWindowThreadProcessId(windowHandle, &windowPid);
					DWORD currentPid = GetCurrentProcessId();
					
					printf("[PopupManager] Window status check for eventId=%d:\n", eventId);
					printf("  - IsWindow: %s\n", isValidWindow ? "TRUE" : "FALSE");
					printf("  - IsVisible: %s\n", isVisible ? "TRUE" : "FALSE");
					printf("  - Window PID: %d, Current PID: %d\n", windowPid, currentPid);
					
					if (isValidWindow) {
						// 【修复】先尝试通过PostMessage发送WM_CLOSE，让窗口自然关闭
						printf("[PopupManager] Sending WM_CLOSE to window for eventId=%d\n", eventId);
						BOOL postResult = PostMessage(windowHandle, WM_CLOSE, 0, 0);
						printf("[PopupManager] PostMessage WM_CLOSE result: %s\n", postResult ? "SUCCESS" : "FAILED");
						
						// 等待一段时间让窗口处理WM_CLOSE
						Sleep(200);
						
						// 检查窗口是否还存在
						if (IsWindow(windowHandle)) {
							printf("[PopupManager] Window still exists after WM_CLOSE, trying direct destroy\n");
							
							// 尝试隐藏窗口
							printf("[PopupManager] Hiding window before destroy for eventId=%d\n", eventId);
							ShowWindow(windowHandle, SW_HIDE);
							
							// 销毁窗口
							printf("[PopupManager] Calling DestroyWindow for eventId=%d\n", eventId);
							BOOL destroyResult = DestroyWindow(windowHandle);
							DWORD lastError = GetLastError();
							
							printf("[PopupManager] DestroyWindow result: %s for eventId=%d\n", 
								destroyResult ? "SUCCESS" : "FAILED", eventId);
							
							if (!destroyResult) {
								printf("[PopupManager] DestroyWindow failed with error: %d for eventId=%d\n", 
									lastError, eventId);
								printf("[PopupManager] Attempting alternative cleanup methods...\n");
								
								// 【新增】尝试强制关闭窗口
								if (IsWindow(windowHandle)) {
									printf("[PopupManager] Trying SetWindowPos to hide window\n");
									SetWindowPos(windowHandle, NULL, 0, 0, 0, 0, 
										SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
								}
							}
							
							// 再次检查窗口是否还存在
							Sleep(100);
							BOOL stillExists = IsWindow(windowHandle);
							printf("[PopupManager] Window still exists after destroy attempt: %s for eventId=%d\n", 
								stillExists ? "TRUE" : "FALSE", eventId);
						} else {
							printf("[PopupManager] Window successfully closed by WM_CLOSE for eventId=%d\n", eventId);
						}
					}
				}
				
				activePopups.erase(it);
				
				printf("[PopupManager] Async cleanup: removed eventId=%d from map\n", eventId);
				printf("[PopupManager] Active popups count after cleanup: %d\n", (int)activePopups.size());
			} else {
				printf("[PopupManager] Warning: eventId=%d not found in activePopups map\n", eventId);
			}
		}).detach();
	}
	
	auto ScheduleWindowDestroy(HWND windowHandle, int eventId) -> void {
		if (windowHandle && IsWindow(windowHandle)) {
			// 延迟销毁窗口，给CEF浏览器时间完成清理
			std::thread([windowHandle, eventId]() {
				Sleep(200);  // 给CEF更多时间
				if (IsWindow(windowHandle)) {
					printf("[PopupManager] Scheduled window destroy for eventId=%d\n", eventId);
					DestroyWindow(windowHandle);
				}
			}).detach();
		}
	}
};