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
	CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

	bool OnQuery(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		int64 query_id, const CefString& request, bool persistent,
		CefRefPtr<CefMessageRouterBrowserSide::Callback> callback);

	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

	// CefLifeSpanHandler methods:
	// ��������������
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	// ������������ر�ʱ����
	// ����false��ʾCEF�Լ������رգ�����true��ʾ�����������ر�
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	// ����������ڱ�����ǰ����
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	// CefLoadHandler methods:
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type) override;
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override;

private:
	CefRefPtr<CefMessageRouterBrowserSide> message_router_;
	CefRefPtr<CefBrowser> cefBrowserHandle;
	HWND cefWindowHandle;
	IMPLEMENT_REFCOUNTING(BrowserClientHandle);
};