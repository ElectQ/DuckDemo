#pragma once
#include "head.h"
#include "v8_api_handle.h"
#include "browser_client.h"
#include "cef_global.h"

//ǧ��Ҫ�޸��������,��������в�����

class SuperDuckAntiVirusCefApp :
	public CefApp,
	public CefBrowserProcessHandler,
	public CefRenderProcessHandler {
public:
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
	
	// 添加命令行开关处理
	virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
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
	// ���캯�������ڽ���������Ҫ�Ĳ���
	CreatePopUpWindowTask(CefRefPtr<SuperDuckAntiVirusCefApp> app,
		int eventId,
		std::wstring malwarePath,
		std::wstring malwareName,
		std::wstring description)
		: app_(app),
		eventId_(eventId),
		malwarePath_(std::move(malwarePath)), // ʹ�� std::move �Ż��ַ�������
		malwareName_(std::move(malwareName)),
		description_(std::move(description)) {}
	// ����CefTask�ӿ�Ҫ��ʵ�ֵ�Ψһ������������Ŀ���߳���ִ��
	void Execute() override {
		// ��UI�߳��ϵ���ʵ�ʵĴ��ڴ�������
		if (app_) {
			app_->CreatePopUpWindow(eventId_, malwarePath_, malwareName_, description_);
		}
	}
private:
	CefRefPtr<SuperDuckAntiVirusCefApp> app_; // ���� CefApp ʵ��������
	int eventId_;
	std::wstring malwarePath_;
	std::wstring malwareName_;
	std::wstring description_;
	// CEF �����ü����꣬�������ӵ����� CefRefPtr �����Ķ�����
	// �⽫�Զ�����������������ڹ���
	IMPLEMENT_REFCOUNTING(CreatePopUpWindowTask);
};
