#pragma once
#include "head.h"
#include "v8_api_handle.h"
#include "browser_client.h"
#include "cef_global.h"

//千万不要修改这个名字,否则就运行不了了

class SuperDuckAntiVirusCefApp :
	public CefApp,
	public CefBrowserProcessHandler,
	public CefRenderProcessHandler {
public:
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
	SuperDuckAntiVirusCefApp() {}

	virtual void OnContextCreated(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context
	) override;
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message);
	//bool CreatePopUpWindow();
	bool CreatePopUpWindow(int eventId,const std::wstring& malwarePath,const std::wstring& malwareName,const std::wstring& description);
	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context);
	virtual void OnContextInitialized() override;
	virtual BOOL SafeCreatePopUpWindow(int EventId, std::wstring malwarePath, std::wstring malwareName, std::wstring description);
private:
	CefRefPtr<CefMessageRouterRendererSide> message_router_;
	IMPLEMENT_REFCOUNTING(SuperDuckAntiVirusCefApp);
};



class CreatePopUpWindowTask : public CefTask {
public:
	// 构造函数，用于接收所有需要的参数
	CreatePopUpWindowTask(CefRefPtr<SuperDuckAntiVirusCefApp> app,
		int eventId,
		std::wstring malwarePath,
		std::wstring malwareName,
		std::wstring description)
		: app_(app),
		eventId_(eventId),
		malwarePath_(std::move(malwarePath)), // 使用 std::move 优化字符串拷贝
		malwareName_(std::move(malwareName)),
		description_(std::move(description)) {}
	// 这是CefTask接口要求实现的唯一方法，它将在目标线程上执行
	void Execute() override {
		// 在UI线程上调用实际的窗口创建函数
		if (app_) {
			app_->CreatePopUpWindow(eventId_, malwarePath_, malwareName_, description_);
		}
	}
private:
	CefRefPtr<SuperDuckAntiVirusCefApp> app_; // 持有 CefApp 实例的引用
	int eventId_;
	std::wstring malwarePath_;
	std::wstring malwareName_;
	std::wstring description_;
	// CEF 的引用计数宏，必须添加到所有 CefRefPtr 管理的对象中
	// 这将自动处理对象的生命周期管理
	IMPLEMENT_REFCOUNTING(CreatePopUpWindowTask);
};
