#include "head.h"


void TestThread(void* ctx) {
	Sleep(1000 * 2);
	auto select = KerneMsg::OnProcessCreate(L"duckTest", L"Ducktestcmd");
	printf("user select: %d \n", select);
}

// 多弹窗测试线程
void MultiPopupTestThread(void* ctx) {
	printf("[MultiPopupTestThread] Starting multi-popup test...\n");
	// 等待CEF和驱动连接完成
	Sleep(1000 * 5);
	
	printf("[MultiPopupTestThread] Starting multiple popup test with 5 windows...\n");
	KerneMsg::TestMultiplePopups(5);
	
	printf("[MultiPopupTestThread] Multi-popup test completed.\n");
}

// 控制台命令监听线程
void ConsoleCommandThread(void* ctx) {
	printf("[ConsoleCommandThread] Console command listener started.\n");
	printf("[ConsoleCommandThread] Available commands:\n");
	printf("  test <number> - Create multiple test popups (e.g., 'test 3')\n");
	printf("  single - Create a single test popup\n");
	printf("  quit - Exit the application\n");
	printf("Enter command: ");
	
	char command[256];
	int count;
	
	while (true) {
		if (fgets(command, sizeof(command), stdin)) {
			// 移除换行符
			command[strcspn(command, "\n")] = 0;
			
			if (strncmp(command, "test ", 5) == 0) {
				if (sscanf_s(command + 5, "%d", &count) == 1) {
					printf("[ConsoleCommandThread] Creating %d test popups...\n", count);
					KerneMsg::TestMultiplePopups(count);
				} else {
					printf("[ConsoleCommandThread] Invalid number. Usage: test <number>\n");
				}
			}
			else if (strcmp(command, "single") == 0) {
				printf("[ConsoleCommandThread] Creating single test popup...\n");
				KerneMsg::OnProcessCreate(L"C:\\Test\\single_test.exe", L"C:\\Test\\single_test.exe /manual");
			}
			else if (strcmp(command, "quit") == 0) {
				printf("[ConsoleCommandThread] Quitting application...\n");
				PostQuitMessage(0);
				break;
			}
			else if (strlen(command) > 0) {
				printf("[ConsoleCommandThread] Unknown command: %s\n", command);
			}
			
			printf("Enter command: ");
		}
		Sleep(100);
	}
}

void ConnectToDriver(void* ctx) {
	//正常来说我们需要R3在一切准备完毕后发一个通知给驱动才开始保护否则驱动会拦截自己,但是算了先这样吧
	printf("[ConnectToDriver] ========== STARTING DRIVER CONNECTION ==========\n");
	printf("[ConnectToDriver] Waiting 3 seconds for CEF initialization...\n");
	Sleep(1000 * 3);
	
	printf("[ConnectToDriver] Attempting to connect to driver...\n");
	if (KerneMsg::Init() == false) {
		printf("[ConnectToDriver] ========== DRIVER CONNECTION FAILED ==========\n");
		printf("驱动链接错误,你犯了一个低级错误");
		return;
	}
	printf("[ConnectToDriver] ========== DRIVER CONNECTION SUCCESS ==========\n");
	
	// 【修复】在独立线程中运行KernMsgRoute，不阻塞主线程
	printf("[ConnectToDriver] Starting KernMsgRoute thread...\n");
	Tools::EasyCreateThread(KerneMsg::KernMsgRoute, nullptr);  //创建与驱动的链接
	printf("[ConnectToDriver] ========== KERNMSGROUTE THREAD LAUNCHED ==========\n");
}


int main() {
	Tools::EasyCreateThread(KerneMsg::KernMsgRoute, nullptr);  //创建与驱动的链接

	SetConsoleOutputCP(CP_UTF8);
	CefMainArgs main_args(GetModuleHandleW(nullptr));
	cef_global::MainCefApp = new SuperDuckAntiVirusCefApp();
	
	int exit_code = CefExecuteProcess(main_args, cef_global::MainCefApp.get(), nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}
	CefSettings settings;
	
	// 禁用GPU加速以解决渲染问题
	settings.windowless_rendering_enabled = false;
	settings.no_sandbox = true;
	
	// 添加命令行开关禁用GPU
	CefString(&settings.cache_path).FromString("cache");
	
	if (!CefInitialize(main_args, settings, cef_global::MainCefApp.get(), nullptr)) {  //efInitialize执行完成后，会自动触发OnContextInitialized回调
		MessageBox(nullptr, L"CEF Initialization Failed!", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	Tools::EasyCreateThread(ConnectToDriver, nullptr);
	
	// 启动控制台命令监听线程（可以手动触发测试）
	//Tools::EasyCreateThread(ConsoleCommandThread, nullptr);
	
	// 自动测试线程（可注释掉此行禁用自动测试）
	// Tools::EasyCreateThread(MultiPopupTestThread, nullptr);
	
	//app->CreatePopUpWindow();  //调用方法触发弹窗
	CefRunMessageLoop(); //它会阻塞当前线程，并开始处理所有CEF相关的事件（如网络请求、页面渲染、JavaScript执行、用户输入等）以及操作系统的消息
	CefShutdown();

	return 0;
}




