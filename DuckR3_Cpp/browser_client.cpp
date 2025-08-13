#include "browser_client.h"
#include "cef_global.h"
#include "kernel_msg.h"
#include <chrono>
#include <map>

// 全局时间记录管理
static std::map<int, std::chrono::steady_clock::time_point> g_eventStartTimes;

void RecordPopupStartTime(int eventId) {
	g_eventStartTimes[eventId] = std::chrono::steady_clock::now();
	printf("[DEBUG] Recorded start time for eventId=%d\n", eventId);
}

// 清理过期的时间记录（避免内存泄漏）
void CleanupEventStartTime(int eventId) {
	auto it = g_eventStartTimes.find(eventId);
	if (it != g_eventStartTimes.end()) {
		printf("[DEBUG] Cleaned up start time for eventId=%d\n", eventId);
		g_eventStartTimes.erase(it);
	}
}

bool BrowserClientHandle::OnQuery(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64 query_id,
	const CefString& request, bool persistent,
	CefRefPtr<CefMessageRouterBrowserSide::Callback> callback) {
	CEF_REQUIRE_UI_THREAD()
		return false;
}
bool BrowserClientHandle::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
	if (message_router_->OnProcessMessageReceived(browser, frame,
		source_process, message)) {
		return true;
	}
	printf("message->GetName(): %s \n", message->GetName().ToString().c_str());

	if (message->GetName() == "ExitProcess") {
		browser->GetHost()->CloseBrowser(true);
		return true;
	}

	if (message->GetName() == "MinimizeWindow") {
		::ShowWindow(
			::GetAncestor(browser->GetHost()->GetWindowHandle(), GA_ROOT),
			SW_MINIMIZE);
		return true;
	}
	if (message->GetName() == "CloseWindow") {
		printf("=== [UI消息接收] CloseWindow ===\n");
		
		// 【修复】直接通过browserID查找eventId，而不依赖URL解析
		PopupInfo* popup = cef_global::GetPopupByBrowserId(browser->GetIdentifier());
		
		if (popup) {
			int eventId = popup->eventId;
			printf("=== [窗口关闭] eventId=%d ===\n", eventId);
			
			// 检查eventIdMap中是否还存在这个eventId（表示还在等待决策）
			bool isWaiting = false;
			{
				std::lock_guard<std::mutex> lock(KerneMsg::theLock);
				isWaiting = (KerneMsg::eventIdMap.find(eventId) != KerneMsg::eventIdMap.end());
			}
			
			if (isWaiting) {
				printf("=== [发送默认阻止决策] eventId=%d ===\n", eventId);
				// 【修复】用户直接关闭窗口时，默认阻止（更安全的行为）
				KerneMsg::HandleCefPopUpMsg(eventId, true); // true = 阻止进程，更安全
			}
		}
		
		// 关闭浏览器
		browser->GetHost()->CloseBrowser(true);
		return true;
	}
	if (message->GetName() == "OnDragZoneMouseDown") {
		SetCapture(cefWindowHandle);
		return true;
	}

	if (message->GetName() == "OpenBrowser") {
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		if (args->GetSize() > 0 && args->GetType(0) == VTYPE_STRING) {
			std::string url = args->GetString(0);
			if (url.compare(0, 7, "http://") == 0 ||
				url.compare(0, 8, "https://") == 0) {
				ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL,
					SW_SHOWNORMAL);
			}
			return true;
		}
		return false;
	}
	if (message->GetName() == "PopSelectAction") {
		printf("==========================================\n");
		printf("=== [UI消息接收] PopSelectAction ===\n");
		printf("==========================================\n");
		printf("[BrowserClient] 🎯 Message received in browser process\n");
		printf("[BrowserClient] Browser ID: %d\n", browser->GetIdentifier());
		printf("[BrowserClient] Source process: %d\n", source_process);
		printf("[BrowserClient] Current thread ID: %lu\n", GetCurrentThreadId());
		
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		if (!args || args->GetSize() < 2) {
			printf("[BrowserClient] ❌ ERROR: Invalid message arguments! Size: %zu\n", 
				args ? args->GetSize() : 0);
			return false;
		}
		
		auto eventId = args.get()->GetInt(0);
		auto eventSelectStatus = args.get()->GetBool(1);

		printf("[BrowserClient] 📥 Parsed message: eventId=%d, isBlock=%s\n", 
			eventId, eventSelectStatus ? "true (BLOCK)" : "false (ALLOW)");

		// 根据决策和时间戳判断消息来源
		const char* decisionStr = eventSelectStatus ? "阻止" : "允许";
		const char* sourceType = "未知";
		
		// 使用全局时间记录判断消息来源
		auto now = std::chrono::steady_clock::now();
		
		// 检查这个eventId是否已经记录了开始时间
		auto timeIt = g_eventStartTimes.find(eventId);
		if (timeIt != g_eventStartTimes.end()) {
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeIt->second).count();
			printf("[DEBUG] ⏰ EventId=%d processed after %lld ms\n", eventId, elapsed);
			
			if (elapsed < 2000) {
				sourceType = "用户快速点击";
			} else if (elapsed >= 17000 && elapsed <= 19000) {
				sourceType = "超时自动处理";
			} else {
				sourceType = "用户主动点击";
			}
		} else {
			sourceType = "未记录开始时间";
		}

		printf("=== [用户决策] 🎯 eventId=%d, 决策=%s, 来源=%s ===\n", 
			eventId, decisionStr, sourceType);
		
		// 清理时间记录，避免内存泄漏
		extern void CleanupEventStartTime(int eventId);
		CleanupEventStartTime(eventId);
		
		printf("[BrowserClient] 🚀 Calling KerneMsg::HandleCefPopUpMsg...\n");
		// 发送用户决策
		KerneMsg::HandleCefPopUpMsg(eventId, eventSelectStatus);
		printf("[BrowserClient] KerneMsg::HandleCefPopUpMsg completed\n");
		
		// 【修复】处理完决策后，主动关闭弹窗浏览器
		printf("[BrowserClient] 🚪 Closing popup browser after processing decision\n");
		browser->GetHost()->CloseBrowser(false);
		
		return true;
	}
	return false;
}
void BrowserClientHandle::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	
	// 检查是否是弹窗浏览器 - 通过窗口句柄判断
	HWND browserWindow = browser->GetHost()->GetWindowHandle();
	HWND parentWindow = GetParent(browserWindow);
	
	// 如果父窗口是主窗口，则这是主浏览器
	if (parentWindow == cef_global::cefWindowHandle && !cefBrowserHandle.get()) {
		// 这是主浏览器
		cefBrowserHandle = browser;

		RECT rect;
		GetClientRect(cefWindowHandle, &rect);
		CefWindowHandle hwndBrowser = cefBrowserHandle->GetHost()->GetWindowHandle();
		if (hwndBrowser) {
			SetWindowPos(hwndBrowser, nullptr, rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top,
				SWP_NOZORDER);
		}
		printf("[BrowserClient] Main browser created with ID: %d, parent window: %p\n", 
			browser->GetIdentifier(), parentWindow);
	}
	else {
		// 这是弹窗浏览器，不要设置为主浏览器
		printf("[BrowserClient] Popup browser created with ID: %d, parent window: %p\n", 
			browser->GetIdentifier(), parentWindow);
		printf("[BrowserClient] Main browser handle preserved (ID: %d)\n", 
			cefBrowserHandle ? cefBrowserHandle->GetIdentifier() : -1);
	}
	message_router_->AddHandler(this, false);
}
bool BrowserClientHandle::DoClose(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	
	// 检查是否是主浏览器
	if (cefBrowserHandle.get() &&
		cefBrowserHandle->GetIdentifier() == browser->GetIdentifier()) {
		printf("[BrowserClient] Main browser closing\n");
		cefBrowserHandle = nullptr;
		// 主浏览器关闭时退出消息循环
		CefQuitMessageLoop();
	}
	else {
		// 这是弹窗浏览器，通过browserID查找对应的弹窗信息
		PopupInfo* popup = cef_global::GetPopupByBrowserId(browser->GetIdentifier());
		if (popup) {
			printf("[BrowserClient] Popup browser closing: eventId=%d, browserID=%d\n", 
				popup->eventId, browser->GetIdentifier());
			// 弹窗关闭时不退出主消息循环
		} else {
			printf("[BrowserClient] Unknown browser closing: browserID=%d\n", browser->GetIdentifier());
		}
	}
	
	message_router_->RemoveHandler(this);
	return false;
}
void BrowserClientHandle::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	
	printf("=== [BROWSER CLOSING] ===\n");
	printf("[BrowserClient] OnBeforeClose called for browser ID: %d\n", browser->GetIdentifier());
	
	// 检查是否是主浏览器
	if (cefBrowserHandle.get() &&
		cefBrowserHandle->GetIdentifier() == browser->GetIdentifier()) {
		printf("[BrowserClient] Main browser OnBeforeClose - this will exit the application\n");
		// 只有主浏览器关闭时才退出消息循环
		CefQuitMessageLoop();
	}
	else {
		printf("[BrowserClient] This is a popup browser, searching in activePopups...\n");
		// 这是弹窗浏览器，通过browserID查找对应的弹窗信息并清理
		PopupInfo* popup = cef_global::GetPopupByBrowserId(browser->GetIdentifier());
		if (popup) {
			int eventId = popup->eventId;
			HWND windowHandle = popup->windowHandle;
			
			printf("[BrowserClient] Found popup: eventId=%d, browserID=%d, windowHandle=%p\n", 
				eventId, browser->GetIdentifier(), windowHandle);
			
			// 检查窗口当前状态
			if (windowHandle) {
				BOOL isValidWindow = IsWindow(windowHandle);
				BOOL isVisible = IsWindowVisible(windowHandle);
				printf("[BrowserClient] Window status before cleanup: Valid=%s, Visible=%s\n", 
					isValidWindow ? "TRUE" : "FALSE", isVisible ? "TRUE" : "FALSE");
				
				// 【修复】直接同步销毁窗口，不使用异步机制
				printf("[BrowserClient] 🚪 Destroying popup window synchronously\n");
				if (isValidWindow) {
					DestroyWindow(windowHandle);
					printf("[BrowserClient] ✅ Window destroyed successfully\n");
				}
			}
			
			// 从弹窗管理器中移除
			printf("[BrowserClient] 🧹 Removing popup from manager\n");
			cef_global::RemovePopup(eventId);
			printf("[BrowserClient] ✅ Popup removed from manager\n");
		} else {
			printf("[BrowserClient] ERROR: Popup not found in activePopups for browserID=%d\n", browser->GetIdentifier());
			printf("[BrowserClient] Current active popups count: %d\n", cef_global::GetActivePopupCount());
		}
	}
	printf("=== [ONBEFORECLOSE COMPLETED] ===\n");
}

// CefLoadHandler methods implementation
void BrowserClientHandle::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type) {
	printf("[DEBUG] Page load started: %s\n", frame->GetURL().ToString().c_str());
}

void BrowserClientHandle::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
	printf("[DEBUG] Page load ended with status %d: %s\n", httpStatusCode, frame->GetURL().ToString().c_str());
	if (httpStatusCode != 200) {
		printf("[WARNING] HTTP status code indicates potential loading issue\n");
	}
}

void BrowserClientHandle::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) {
	printf("[ERROR] Failed to load URL: %s\n", failedUrl.ToString().c_str());
	printf("[ERROR] Error code: %d, Error text: %s\n", errorCode, errorText.ToString().c_str());
	
	// 常见错误处理
	if (errorCode == ERR_CONNECTION_REFUSED) {
		printf("[ERROR] Connection refused - Is the React development server running on localhost:3000?\n");
	}
}