#include "main_app.h"
#include <windowsx.h>

LRESULT CALLBACK Cef_CreateEventWindowMsgHandle(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam) {
	static int xClick, yClick;

	switch (message) {
	case WM_SIZE:
		if (cef_global::cefBrowserEventHandle.get()) {
			// 当窗口大小改变时，通知CEF浏览器调整大小
			CefWindowHandle hwndBrowser =
				cef_global::cefBrowserEventHandle->GetHost()
				->GetWindowHandle();
			if (hwndBrowser) {
				RECT rect;
				GetClientRect(hWnd, &rect);
				SetWindowPos(hwndBrowser, nullptr, rect.left, rect.top,
					rect.right - rect.left, rect.bottom - rect.top,
					SWP_NOZORDER);
			}
		}
		return 0;
	case WM_ERASEBKGND:
		// 防止背景闪烁
		if (cef_global::cefBrowserEventHandle.get()) {
			return 1;
		}
		break;
	case WM_CLOSE:
		if (cef_global::cefBrowserEventHandle.get()) {
			// 请求浏览器关闭
			cef_global::cefBrowserEventHandle->GetHost()->CloseBrowser(
				false);
			// 不立即销毁窗口，等待 OnBeforeClose
			return 0;
		}
		break;
	case WM_PARENTNOTIFY:
		if (WM_LBUTTONDOWN == wParam) {
			xClick = GET_X_LPARAM(lParam);
			yClick = GET_Y_LPARAM(lParam);
			return 0;
		}
		break;
	case WM_MOUSEMOVE: {
		if (GetCapture() == hWnd) {
			RECT rect;
			GetWindowRect(hWnd, &rect);

			POINT point;
			point.x = GET_X_LPARAM(lParam);
			point.y = GET_Y_LPARAM(lParam);

			int xWindow = rect.left + point.x - xClick;
			int yWindow = rect.top + point.y - yClick;
			SetWindowPos(hWnd, nullptr, xWindow, yWindow, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER);
		}
		return 0;
	}

	case WM_DISPLAYCHANGE: {
		int newWidth = LOWORD(lParam);
		int newHeight = HIWORD(lParam);

		int browserHeight = newHeight / 2;
		int browserWidth = browserHeight + browserHeight / 2;

		printf("[WM_DISPLAYCHANGE] w:%d h:%d\n", browserWidth,
			browserHeight);

		SetWindowPos(hWnd, nullptr, 0, 0, browserWidth, browserHeight,
			SWP_NOMOVE | SWP_NOZORDER);
		return 0;
	}
	case WM_LBUTTONUP:
		ReleaseCapture();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);

		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
LRESULT CALLBACK Cef_CreateWindowMsgHandle(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam) {
	static int xClick, yClick;

	switch (message) {
	case WM_SIZE:
		if (cef_global::cefBrowserHandle.get()) {
			// 当窗口大小改变时，通知CEF浏览器调整大小
			CefWindowHandle hwndBrowser =
				cef_global::cefBrowserHandle->GetHost()->GetWindowHandle();
			if (hwndBrowser) {
				RECT rect;
				GetClientRect(hWnd, &rect);
				SetWindowPos(hwndBrowser, nullptr, rect.left, rect.top,
					rect.right - rect.left, rect.bottom - rect.top,
					SWP_NOZORDER);
			}
		}
		return 0;
	case WM_ERASEBKGND:
		// 防止背景闪烁
		if (cef_global::cefBrowserHandle.get()) {
			return 1;
		}
		break;
	case WM_CLOSE:
		if (cef_global::cefBrowserHandle.get()) {
			// 请求浏览器关闭
			cef_global::cefBrowserHandle->GetHost()->CloseBrowser(false);
			// 不立即销毁窗口，等待 OnBeforeClose
			return 0;
		}
		break;
	case WM_PARENTNOTIFY:
		if (WM_LBUTTONDOWN == wParam) {
			xClick = GET_X_LPARAM(lParam);
			yClick = GET_Y_LPARAM(lParam);
			return 0;
		}
		break;
	case WM_MOUSEMOVE: {
		if (GetCapture() == hWnd) {
			RECT rect;
			GetWindowRect(hWnd, &rect);

			POINT point;
			point.x = GET_X_LPARAM(lParam);
			point.y = GET_Y_LPARAM(lParam);

			int xWindow = rect.left + point.x - xClick;
			int yWindow = rect.top + point.y - yClick;
			SetWindowPos(hWnd, nullptr, xWindow, yWindow, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER);
		}
		return 0;
	}

	case WM_DISPLAYCHANGE: {
		int newWidth = LOWORD(lParam);
		int newHeight = HIWORD(lParam);

		int browserHeight = newHeight / 2;
		int browserWidth = browserHeight + browserHeight / 2;

		printf("[WM_DISPLAYCHANGE] w:%d h:%d\n", browserWidth,
			browserHeight);

		SetWindowPos(hWnd, nullptr, 0, 0, browserWidth, browserHeight,
			SWP_NOMOVE | SWP_NOZORDER);
		return 0;
	}
	case WM_LBUTTONUP:
		ReleaseCapture();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);

		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
LRESULT CALLBACK CefBrowserWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam) {
	return CallWindowProcW(cef_global::cefBrowserWindowProc, hWnd, uMsg, wParam,
		lParam);
}
LRESULT CALLBACK CefBrowserEventWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam) {
	return CallWindowProcW(cef_global::cefBrowserEventWindowProc, hWnd, uMsg,
		wParam, lParam);
}


void SuperDuckAntiVirusCefApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefV8Context> context) {
	CefRefPtr<CefV8Value> global = context->GetGlobal();
	CefRefPtr<CefV8Handler> handler = new ApiV8Handler();

	CefRefPtr<CefV8Value> function;

	// 退出进程
	function = CefV8Value::CreateFunction("NativeApi_ExitProcess", handler);
	global->SetValue("NativeApi_ExitProcess", function, V8_PROPERTY_ATTRIBUTE_NONE);

	// 拖拽区域鼠标按下
	function = CefV8Value::CreateFunction("NativeApi_OnDragZoneMouseDown", handler);
	global->SetValue("NativeApi_OnDragZoneMouseDown", function,
		V8_PROPERTY_ATTRIBUTE_NONE);

	// 拖拽区域鼠标释放
	function = CefV8Value::CreateFunction("NativeApi_OnDragZoneMouseUp", handler);
	global->SetValue("NativeApi_OnDragZoneMouseUp", function, V8_PROPERTY_ATTRIBUTE_NONE);

	// 最小化窗口
	function = CefV8Value::CreateFunction("NativeApi_MinimizeWindow", handler);
	global->SetValue("NativeApi_MinimizeWindow", function, V8_PROPERTY_ATTRIBUTE_NONE);

	// 关闭窗口
	function = CefV8Value::CreateFunction("NativeApi_CloseWindow", handler);
	global->SetValue("NativeApi_CloseWindow", function, V8_PROPERTY_ATTRIBUTE_NONE);

	// 消息路由器配置
	CefMessageRouterConfig config;
	message_router_ = CefMessageRouterRendererSide::Create(config);
	message_router_->OnContextCreated(browser, frame, context);
}

bool SuperDuckAntiVirusCefApp::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
	return message_router_->OnProcessMessageReceived(browser, frame,
		source_process, message);
}
#include <iomanip> // For std::hex, std::setw, std::setfill

std::string urlEncodeQueryValue(const std::string& value) {
	std::ostringstream escaped;
	escaped.fill('0'); // 填充0，例如 %0A
	escaped << std::hex; // 使用十六进制
	for (unsigned char c : value) {
		// 根据RFC 3986，这些字符不需要编码
		// (字母、数字、-、_、.、~)
		if ((c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
		}
		else {
			// 其他字符（包括多字节UTF-8字符的各个字节）需要编码
			escaped << '%' << std::setw(2) << static_cast<int>(c);
		}
	}
	return escaped.str();
}

bool SuperDuckAntiVirusCefApp::CreatePopUpWindow(int eventId,
	const std::wstring& malwarePath,
	const std::wstring& malwareName,
	const std::wstring& description) {
	UINT currentSystemDpi = GetDpiForSystem();
	int windowWidth = MulDiv(500, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int windowHeight = MulDiv(360, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);

	// 获取主屏幕的宽度和高度
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 计算窗口居中位置
	int posX = (screenWidth - windowWidth) / 2;
	int posY = (screenHeight - windowHeight) / 2;

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = Cef_CreateEventWindowMsgHandle;
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = L"SafeDuckYaEvent";
	wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	RegisterClassEx(&wcex);

	DWORD dwStyle = WS_POPUP | WS_CLIPCHILDREN;
	cef_global::cefEventWindowHandle = CreateWindow(L"SafeDuckYaEvent", L"超级安全鸭弹窗",
		dwStyle, posX, posY, windowWidth, windowHeight,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	ShowWindow(cef_global::cefEventWindowHandle, SW_SHOW);
	UpdateWindow(cef_global::cefEventWindowHandle);

	CefWindowInfo window_info;
	RECT rect;
	GetClientRect(cef_global::cefEventWindowHandle, &rect);

	// CEF 浏览器应该填充整个父窗口的客户端区域
	window_info.bounds.x = rect.left;
	window_info.bounds.y = rect.top;
	window_info.bounds.width = rect.right - rect.left;
	window_info.bounds.height = rect.bottom - rect.top;
	window_info.SetAsChild(cef_global::cefEventWindowHandle, (const CefRect&)window_info.bounds);

	CefBrowserSettings browser_settings;

	// 🎯 使用传入的参数构建URL
	std::wstring url = L"http://localhost:3000/virus-alert?eventId=" + std::to_wstring(eventId) +
		L"&filePath=" + malwarePath +
		L"&virusName=" + malwareName +
		L"&description=" + description;

	CefRefPtr<CefClient> browserClient(new BrowserClientHandle(cef_global::cefBrowserEventHandle, cef_global::cefEventWindowHandle));
	auto browser = CefBrowserHost::CreateBrowserSync(window_info, browserClient, url, browser_settings, nullptr, nullptr);

	cef_global::cefBrowserEventWindowProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(browser->GetHost()->GetWindowHandle(), GWLP_WNDPROC));
	SetWindowLongPtrW(browser->GetHost()->GetWindowHandle(), GWLP_WNDPROC, reinterpret_cast<ULONG_PTR>(CefBrowserEventWindowProc));

	return true;
}



void SuperDuckAntiVirusCefApp::OnContextInitialized() {  //CEF初始化重构方法接入我们REACT前端
	CEF_REQUIRE_UI_THREAD();

	// 确保主窗口只创建一次
	if (cef_global::cefWindowHandle) {
		return;
	}

	// 先算DPI
	UINT currentSystemDpi = GetDpiForSystem();

	int windowWidth =MulDiv(LOGICAL_MIN_WIDTH,MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int windowHeight =MulDiv(LOGICAL_MIN_HEIGHT,MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);

	// 获取主屏幕的宽度和高度
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 计算窗口居中位置
	int posX = (screenWidth - windowWidth) / 2;
	int posY = (screenHeight - windowHeight) / 2;

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;  //重绘整个窗口
	wcex.lpfnWndProc = Cef_CreateWindowMsgHandle;  //注册函数
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = L"SafeDuckYa";  //窗口类名
	wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	RegisterClassEx(&wcex);
	DWORD dwStyle = WS_POPUP | WS_CLIPCHILDREN; 

	// 创建主窗口，使用计算出的 posX, posY 和 windowWidth, windowHeight
	cef_global::cefWindowHandle = CreateWindow(L"SafeDuckYa", L"超级安全鸭", dwStyle, posX, posY,windowWidth, windowHeight,nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

	if (!cef_global::cefWindowHandle) {
		MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_OK | MB_ICONERROR);
		return;
	}

	//CEF浏览器嵌入原生的windows窗口
	ShowWindow(cef_global::cefWindowHandle, SW_SHOW);
	UpdateWindow(cef_global::cefWindowHandle);

	CefWindowInfo window_info;
	RECT rect;
	GetClientRect(cef_global::cefWindowHandle,&rect); 
	int displayHeight = GetSystemMetrics(SM_CYSCREEN);
	// CEF 浏览器应该填充整个父窗口的客户端区域
	window_info.bounds.x = rect.left;
	window_info.bounds.y = rect.top;
	window_info.bounds.width = rect.right - rect.left;
	window_info.bounds.height = rect.bottom - rect.top;

	window_info.SetAsChild(cef_global::cefWindowHandle,(const CefRect&)window_info.bounds); 

	CefBrowserSettings browser_settings;
	
	
	std::string url = "http://localhost:3000/";
	CefBrowserSettings browserSettings;
	CefRefPtr<CefClient> browserClient(new BrowserClientHandle(cef_global::cefBrowserHandle, cef_global::cefWindowHandle));  //BrowserClientHandle处理浏览器事件 efClient在CreateBrowserSync执行时就开始部分生效
	auto browser = CefBrowserHost::CreateBrowserSync(window_info, browserClient, url, browser_settings, nullptr, nullptr);  //CreateBrowserSync会阻塞直到浏览器创建完成
	cef_global::cefBrowserWindowProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(browser->GetHost()->GetWindowHandle(), GWLP_WNDPROC));
	SetWindowLongPtrW(browser->GetHost()->GetWindowHandle(), GWLP_WNDPROC,reinterpret_cast<ULONG_PTR>(CefBrowserWindowProc));  //自定义处理后，仍调用原始处理函数
}  


void SuperDuckAntiVirusCefApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,CefRefPtr<CefV8Context> context) {
	message_router_->OnContextReleased(browser, frame, context);
}

BOOL SuperDuckAntiVirusCefApp::SafeCreatePopUpWindow(int EventId,
	std::wstring malwarePath,
	std::wstring malwareName,
	std::wstring description) {
	// 检查当前是否已经在UI线程，如果不是则PostTask
	 // 确保 cef_global::MainCefApp 是有效且非空的，否则会崩溃
	if (!cef_global::MainCefApp) {
		// 应用程序未正确初始化或 MainCefApp 为空
		OutputDebugString(L"Error: cef_global::MainCefApp is null in SafeCreatePopUpWindow!\n");
		return FALSE;
	}
	// 检查当前是否已经在UI线程，如果不是则PostTask
	if (CefCurrentlyOn(TID_UI)) {
		// 如果当前已经在UI线程，直接调用 CreatePopUpWindow
		return cef_global::MainCefApp->CreatePopUpWindow(EventId, malwarePath, malwareName, description);
	}
	else {
		// 创建任务实例，并将其包装在 CefRefPtr 中
		CefRefPtr<CreatePopUpWindowTask> task = new CreatePopUpWindowTask(
			cef_global::MainCefApp, // 传递 CefApp 实例
			EventId,
			malwarePath,
			malwareName,
			description
		);
		// 使用 CefPostTask 将任务发布到 UI 线程
		CefPostTask(TID_UI, task);
		// 注意：CefPostTask 是异步的，它只表示任务被成功调度，不表示窗口已经创建。
		return TRUE; // 表示任务已成功调度
	}
}

// 这是你需要传递给CefPostTask的任务类



