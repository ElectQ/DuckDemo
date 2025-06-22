#include "browser_client.h"

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
		browser->GetHost()->CloseBrowser(true);
		PostMessageW(cef_global::cefEventWindowHandle, WM_CLOSE, 0, 0);
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
		CefRefPtr<CefListValue> args = message->GetArgumentList();

		auto eventId = args.get()->GetInt(0);
		auto eventSelectStatus = args.get()->GetBool(1);

		KerneMsg::HandleCefPopUpMsg(eventId, eventSelectStatus);
		return true;
	}
	return false;
}
void BrowserClientHandle::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	if (!cefBrowserHandle.get()) {
		cefBrowserHandle = browser;

		RECT rect;
		GetClientRect(cefWindowHandle,
			&rect);
		CefWindowHandle hwndBrowser =
			cefBrowserHandle->GetHost()->GetWindowHandle();
		if (hwndBrowser) {
			SetWindowPos(hwndBrowser, nullptr, rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top,
				SWP_NOZORDER);
		}
	}
	message_router_->AddHandler(this, false);
}
bool BrowserClientHandle::DoClose(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	// 如果是主浏览器，允许关闭
	if (cefBrowserHandle.get() &&
		cefBrowserHandle->GetIdentifier() ==
		browser->GetIdentifier()) {
		cefBrowserHandle = nullptr;
		// 可以选择在这里 PostQuitMessage(0) 如果这是最后一个浏览器
	}
	message_router_->RemoveHandler(this);
	CefQuitMessageLoop();
	return false;
}
void BrowserClientHandle::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	CEF_REQUIRE_UI_THREAD();
	if (cefBrowserHandle.get() &&
		cefBrowserHandle->GetIdentifier() ==
		browser->GetIdentifier()) {
		// 如果这是主浏览器且它是最后一个浏览器，则退出消息循环
		// （在这个简单例子中，我们只有一个浏览器）
		CefQuitMessageLoop();
	}
}