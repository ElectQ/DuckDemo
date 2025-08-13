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
	{
		printf("[WindowMsg] WM_CLOSE received for popup window %p\n", hWnd);
		
		// 【修复】简化关闭逻辑，直接允许窗口关闭
		// CEF浏览器会在OnBeforeClose中处理清理工作
		printf("[WindowMsg] Allowing popup window %p to close normally\n", hWnd);
		break; // 允许默认处理
	}
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
		printf("[WindowMsg] WM_DESTROY received for popup window %p\n", hWnd);
		printf("[WindowMsg] Popup window %p is being destroyed by Windows\n", hWnd);
		// 【修复】弹窗不应该调用PostQuitMessage，只有主窗口才应该调用
		// PostQuitMessage(0); // 移除这行，避免弹窗关闭导致整个应用退出

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
	case WM_USER + 100: // 处理异步弹窗创建消息
	{
		printf("[WindowMsg] ==========================================\n");
		printf("[WindowMsg] 🎯 Received WM_USER+100 for asynchronous popup creation\n");
		printf("[WindowMsg] Current thread ID: %lu\n", GetCurrentThreadId());
		printf("[WindowMsg] Window handle: %p\n", hWnd);
		printf("[WindowMsg] ==========================================\n");
		
		struct PopupParams {
			SuperDuckAntiVirusCefApp* app;
			int eventId;
			std::wstring* malwarePath;
			std::wstring* malwareName;
			std::wstring* description;
		};
		
		PopupParams* params = reinterpret_cast<PopupParams*>(wParam);
		if (params && params->app && params->malwarePath && params->malwareName && params->description) {
			printf("[WindowMsg] ✅ Valid popup parameters received\n");
			printf("[WindowMsg] - App pointer: %p\n", params->app);
			printf("[WindowMsg] - EventId: %d\n", params->eventId);
			printf("[WindowMsg] 🚀 Executing CreatePopUpWindow asynchronously on UI thread\n");
			
			BOOL result = params->app->CreatePopUpWindow(
				params->eventId,
				*params->malwarePath,
				*params->malwareName,
				*params->description
			);
			
			printf("[WindowMsg] ✅ CreatePopUpWindow completed with result: %d\n", result);
			
			// 清理动态分配的内存
			printf("[WindowMsg] 🧹 Cleaning up dynamically allocated parameters\n");
			delete params->malwarePath;
			delete params->malwareName;
			delete params->description;
			delete params;
			
			printf("[WindowMsg] 🏁 Async popup creation finished\n");
			return static_cast<LRESULT>(result);
		} else {
			printf("[WindowMsg] ❌ ERROR: Invalid popup parameters\n");
			printf("[WindowMsg] - params pointer: %p\n", params);
			if (params) {
				printf("[WindowMsg] - app pointer: %p\n", params->app);
				printf("[WindowMsg] - malwarePath pointer: %p\n", params->malwarePath);
				printf("[WindowMsg] - malwareName pointer: %p\n", params->malwareName);
				printf("[WindowMsg] - description pointer: %p\n", params->description);
				
				// 清理可能的内存泄漏
				if (params->malwarePath) delete params->malwarePath;
				if (params->malwareName) delete params->malwareName;
				if (params->description) delete params->description;
				delete params;
			}
			return FALSE;
		}
	}
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

void SuperDuckAntiVirusCefApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) {
	// 禁用GPU相关功能以解决渲染问题
	command_line->AppendSwitch("--disable-gpu");
	command_line->AppendSwitch("--disable-gpu-compositing");
	command_line->AppendSwitch("--disable-gpu-sandbox");
	command_line->AppendSwitch("--disable-software-rasterizer");
	command_line->AppendSwitch("--disable-background-timer-throttling");
	command_line->AppendSwitch("--disable-renderer-backgrounding");
	command_line->AppendSwitch("--disable-features=TranslateUI");
	command_line->AppendSwitch("--disable-extensions");
	command_line->AppendSwitch("--no-sandbox");
	
	printf("[DEBUG] Command line switches added to disable GPU acceleration\n");
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
	printf("[OnContextCreated] Context created for browser ID: %d, URL: %s\n", 
		browser->GetIdentifier(), frame->GetURL().ToString().c_str());
		
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

	// 【新增】日志输出到控制台
	function = CefV8Value::CreateFunction("NativeApi_LogToConsole", handler);
	global->SetValue("NativeApi_LogToConsole", function, V8_PROPERTY_ATTRIBUTE_NONE);
	printf("[OnContextCreated] ✅ NativeApi_LogToConsole registered for browser ID: %d\n", browser->GetIdentifier());

	// 【重要】弹窗选择动作 - 用户点击允许/阻止按钮
	function = CefV8Value::CreateFunction("NativeApi_PopSelectAction", handler);
	global->SetValue("NativeApi_PopSelectAction", function, V8_PROPERTY_ATTRIBUTE_NONE);
	printf("[OnContextCreated] ✅ NativeApi_PopSelectAction registered for browser ID: %d\n", browser->GetIdentifier());

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
	printf("[MainApp] CreatePopUpWindow called for eventId=%d, path=%ws\n", eventId, malwarePath.c_str());
	
	// 检查是否已达到最大弹窗数量
	if (cef_global::GetActivePopupCount() >= cef_global::MAX_CONCURRENT_POPUPS) {
		printf("[MainApp] Maximum concurrent popups (%d) reached, refusing to create new popup\n", 
			cef_global::MAX_CONCURRENT_POPUPS);
		return false;
	}
	
	UINT currentSystemDpi = GetDpiForSystem();
	
	// 【优化】根据内容动态调整窗口大小，设置最小和最大尺寸
	int baseWindowWidth = 600;   // 增加基础宽度，适应更长的路径
	int baseWindowHeight = 400;  // 增加基础高度，减少滚动条
	
	// 根据文件路径长度动态调整宽度
	int pathLength = static_cast<int>(malwarePath.length());
	if (pathLength > 50) {
		baseWindowWidth = (std::min)(800, baseWindowWidth + (pathLength - 50) * 3); // 每超出1个字符增加3像素
	}
	
	// 应用DPI缩放
	int windowWidth = MulDiv(baseWindowWidth, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int windowHeight = MulDiv(baseWindowHeight, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	
	// 【新增】设置最小和最大窗口尺寸，避免过小或过大
	int minWidth = MulDiv(500, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int maxWidth = MulDiv(1000, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int minHeight = MulDiv(350, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	int maxHeight = MulDiv(600, MulDiv(currentSystemDpi, 100, USER_DEFAULT_SCREEN_DPI), 100);
	
	windowWidth = (std::max)(minWidth, (std::min)(maxWidth, windowWidth));
	windowHeight = (std::max)(minHeight, (std::min)(maxHeight, windowHeight));

	// 获取主屏幕的宽度和高度
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 计算窗口位置 - 为多个弹窗错开位置
	int activeCount = cef_global::GetActivePopupCount();
	int offsetX = activeCount * 50;  // 每个弹窗右移50像素
	int offsetY = activeCount * 50;  // 每个弹窗下移50像素
	
	int posX = (screenWidth - windowWidth) / 2 + offsetX;
	int posY = (screenHeight - windowHeight) / 2 + offsetY;
	
	// 确保窗口不会超出屏幕边界
	if (posX + windowWidth > screenWidth) posX = screenWidth - windowWidth - 10;
	if (posY + windowHeight > screenHeight) posY = screenHeight - windowHeight - 10;

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

	// 【优化】窗口样式：禁用调整大小，确保固定布局
	DWORD dwStyle = WS_POPUP | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU;
	DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW; // 始终置顶，不在任务栏显示
	
	// 创建唯一的窗口标题
	std::wstring windowTitle = L"超级鸭弹窗 #" + std::to_wstring(eventId);
	
	HWND eventWindowHandle = CreateWindowEx(dwExStyle, L"SafeDuckYaEvent", windowTitle.c_str(),
		dwStyle, posX, posY, windowWidth, windowHeight,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
		
	if (!eventWindowHandle) {
		printf("[MainApp] Failed to create window for eventId=%d\n", eventId);
		return false;
	}
	
	printf("[MainApp] Created popup window: eventId=%d, handle=%p\n", 
		eventId, eventWindowHandle);
	
	// 记录弹窗开始时间，用于后续判断消息来源
	extern void RecordPopupStartTime(int eventId);
	RecordPopupStartTime(eventId);
	
	ShowWindow(eventWindowHandle, SW_SHOW);
	UpdateWindow(eventWindowHandle);

	CefWindowInfo window_info;
	RECT rect;
	GetClientRect(eventWindowHandle, &rect);

	// CEF 浏览器应该填充整个父窗口的客户端区域
	window_info.bounds.x = rect.left;
	window_info.bounds.y = rect.top;
	window_info.bounds.width = rect.right - rect.left;
	window_info.bounds.height = rect.bottom - rect.top;
	window_info.SetAsChild(eventWindowHandle, (const CefRect&)window_info.bounds);

	CefBrowserSettings browser_settings;
	
	// 【重要】设置背景色为白色，防止加载时显示黑色
	browser_settings.background_color = CefColorSetARGB(255, 255, 255, 255);
	
	// 【优化】设置合适的字体大小，避免内容过小
	browser_settings.minimum_font_size = 12;
	browser_settings.default_font_size = 14;
	
	// 【新增】禁用JavaScript关闭窗口功能
	browser_settings.javascript_close_windows = STATE_DISABLED;

	// 【修改】恢复使用原来的告警弹窗，使用正确的React路由路径
	std::string malwarePathUtf8(malwarePath.begin(), malwarePath.end());
	std::string malwareNameUtf8(malwareName.begin(), malwareName.end());
	std::string descriptionUtf8(description.begin(), description.end());
	
	std::string url = "http://192.168.36.1:3000/alert-popup?eventId=" + std::to_string(eventId) +
		"&filePath=" + urlEncodeQueryValue(malwarePathUtf8) +
		"&virusName=" + urlEncodeQueryValue(malwareNameUtf8) +
		"&description=" + urlEncodeQueryValue(descriptionUtf8) +
		"&debug=true"; // 添加调试标志

	// 调试输出URL
	printf("[DEBUG] Creating popup window with URL: %s\n", url.c_str());
	printf("[DEBUG] URL length: %zu characters\n", url.length());

	// 先将弹窗信息添加到管理器（使用临时浏览器引用）
	cef_global::AddPopup(eventId, eventWindowHandle, nullptr);

	CefRefPtr<CefClient> browserClient(new BrowserClientHandle(nullptr, eventWindowHandle));
	auto browser = CefBrowserHost::CreateBrowserSync(window_info, browserClient, url, browser_settings, nullptr, nullptr);
	
	// 检查浏览器是否创建成功
	if (!browser) {
		cef_global::RemovePopup(eventId);  // 移除预注册的弹窗
		DestroyWindow(eventWindowHandle);
		return false;
	}
	
	// 更新弹窗管理器中的浏览器引用
	{
		std::lock_guard<std::mutex> lock(cef_global::popupMapMutex);
		auto it = cef_global::activePopups.find(eventId);
		if (it != cef_global::activePopups.end()) {
			it->second.browserHandle = browser;  // 更新浏览器引用
		}
	}

	// 设置窗口过程（每个弹窗独立）
	WNDPROC oldProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(browser->GetHost()->GetWindowHandle(), GWLP_WNDPROC));
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
	
	
	std::string url = "http://192.168.36.1:3000/";
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
	printf("[MainApp] SafeCreatePopUpWindow called for eventId=%d\n", EventId);
	
	// 确保 cef_global::MainCefApp 是有效且非空的，否则会崩溃
	if (!cef_global::MainCefApp) {
		// 应用程序未正确初始化或 MainCefApp 为空
		OutputDebugString(L"Error: cef_global::MainCefApp is null in SafeCreatePopUpWindow!\n");
		return FALSE;
	}
	
	// 【修复】始终在UI线程同步执行，避免异步任务导致的竞态条件
	if (CefCurrentlyOn(TID_UI)) {
		// 如果当前已经在UI线程，直接调用 CreatePopUpWindow
		printf("[MainApp] Already on UI thread, calling CreatePopUpWindow directly\n");
		return cef_global::MainCefApp->CreatePopUpWindow(EventId, malwarePath, malwareName, description);
	}
	else {
		printf("[MainApp] Not on UI thread, using PostMessage for asynchronous execution\n");
		
		// 【关键修复】使用 PostMessage 异步调用，避免阻塞主窗口
		// 动态分配参数结构体，在UI线程处理完成后释放
		struct PopupParams {
			SuperDuckAntiVirusCefApp* app;
			int eventId;
			std::wstring* malwarePath;
			std::wstring* malwareName;
			std::wstring* description;
		};
		
		PopupParams* params = new PopupParams{
			cef_global::MainCefApp.get(),
			EventId,
			new std::wstring(malwarePath),
			new std::wstring(malwareName),
			new std::wstring(description)
		};
		
		// 使用 PostMessage 异步调用到主线程，不阻塞当前线程
		if (cef_global::cefWindowHandle) {
			printf("[MainApp] 🎯 Posting WM_USER+100 message to main window handle: %p\n", cef_global::cefWindowHandle);
			printf("[MainApp] 📦 Popup parameters: eventId=%d\n", EventId);
			printf("[MainApp] 🚀 Starting asynchronous PostMessage call...\n");
			
			// 发送异步消息到主窗口，不阻塞当前线程
			BOOL result = PostMessage(cef_global::cefWindowHandle, WM_USER + 100, 
				reinterpret_cast<WPARAM>(params), 0);
				
			printf("[MainApp] ✅ PostMessage completed with result: %s\n", result ? "SUCCESS" : "FAILED");
			printf("[MainApp] 🏁 SafeCreatePopUpWindow finished (async)\n");
			return result;
		} else {
			printf("[MainApp] ❌ ERROR: Main window handle is null, cannot create popup\n");
			// 清理动态分配的内存
			delete params->malwarePath;
			delete params->malwareName;
			delete params->description;
			delete params;
			return FALSE;
		}
	}
}


