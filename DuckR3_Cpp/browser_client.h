#pragma once
#include "main_app.h"

class BrowserClientHandle : public CefClient,
	public CefLifeSpanHandler,
	public CefDisplayHandler,
	public CefContextMenuHandler,
	public CefLoadHandler,
	public CefRequestHandler,
	public CefMessageRouterBrowserSide::Handler {
public:
	BrowserClientHandle(CefRefPtr<CefBrowser> browserHandle, HWND windowHandle) {
		CefMessageRouterConfig config;
		message_router_ = CefMessageRouterBrowserSide::Create(config);
		cefBrowserHandle = browserHandle;
		cefWindowHandle = windowHandle;
	}

	// CefClient methods:
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

	bool OnQuery(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		int64 query_id, const CefString& request, bool persistent,
		CefRefPtr<CefMessageRouterBrowserSide::Callback> callback);

	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

	// CefLifeSpanHandler methods:
	// 浏览器创建后调用
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	// 当浏览器即将关闭时调用
	// 返回false表示CEF自己处理关闭，返回true表示宿主程序处理关闭
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	// 在浏览器窗口被销毁前调用
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

private:
	CefRefPtr<CefMessageRouterBrowserSide> message_router_;
	CefRefPtr<CefBrowser> cefBrowserHandle;
	HWND cefWindowHandle;
	IMPLEMENT_REFCOUNTING(BrowserClientHandle);
};