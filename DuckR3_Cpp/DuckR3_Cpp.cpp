#include "head.h"


void TestThread(void* ctx) {
	Sleep(1000 * 2);
	auto select = KerneMsg::OnProcessCreate(L"duckTest", L"Ducktestcmd");
	printf("user select: %d \n", select);
}

int main() {
	SetConsoleOutputCP(CP_UTF8);
	CefMainArgs main_args(GetModuleHandleW(nullptr));
	cef_global::MainCefApp = new SuperDuckAntiVirusCefApp();
	//CefRefPtr<SuperDuckAntiVirusCefApp> app(new SuperDuckAntiVirusCefApp());  //智能指针创建实例
	int exit_code = CefExecuteProcess(main_args, cef_global::MainCefApp.get(), nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}
	CefSettings settings;
	if (!CefInitialize(main_args, settings, cef_global::MainCefApp.get(), nullptr)) {  //efInitialize执行完成后，会自动触发OnContextInitialized回调
		MessageBox(nullptr, L"CEF Initialization Failed!", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	//app->CreatePopUpWindow();  //调用方法触发弹窗
	CefRunMessageLoop(); //它会阻塞当前线程，并开始处理所有CEF相关的事件（如网络请求、页面渲染、JavaScript执行、用户输入等）以及操作系统的消息
	CefShutdown();

	return 0;
}




