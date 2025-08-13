#include "kernel_msg.h"
#include "cef_global.h"


namespace KerneMsg {
	int currentIndex = 0;;
	std::map<int, std::tuple<void*, bool*>> eventIdMap; //用并发安全的
	std::mutex theLock;
	enum class _ClientMsgType { kCreateProcess, kReadFile, kWriteFile };
	struct _MsgClientPacket {
		uint32_t magicNum;
		_ClientMsgType type;
	};

	struct _MsgCreateProcess {
		_MsgClientPacket packet;
		uint32_t pid;
		uint32_t ppid;
		size_t pathSize;
		size_t commandSize;
		size_t commandOffset;

		wchar_t path[1];
		wchar_t commandLine[1];
	};
	struct _MsgFileRw {
		_MsgClientPacket packetHeader;
		uint32_t pid;
		wchar_t path[1];
	};
	_declspec(align(8)) struct _Msg_CreateProcess_R3Reply {
		FILTER_REPLY_HEADER replyHeader;
		bool isNeedBlock;
	};

	static const auto Ring3CommPort = L"\\DuckGuardCommPort";
	HANDLE minifilterHandle = INVALID_HANDLE_VALUE;

	auto OnProcessCreate(std::wstring Path, std::wstring CommandLine) -> bool {
		printf("=== [进程创建告警] %ws ===\n", Path.c_str());
		if (Path.find(L"DuckR3_Cpp") != -1) {
			return false;  // 忽略自身进程
		}
		
		auto eventHandle = CreateEventW(NULL, FALSE, FALSE, NULL);
		if (eventHandle == INVALID_HANDLE_VALUE) {
			return false;
		}
		bool isNeedBlock = false;
		theLock.lock();
		auto index = currentIndex;
		eventIdMap[index] = { eventHandle , &isNeedBlock };

		currentIndex += 1;
		if (currentIndex > 0x1337) {
			currentIndex = 0;
		}
		theLock.unlock();
		
		// 等待弹窗槽位可用
		while (true) {
			std::lock_guard<std::mutex> lock(cef_global::popupMapMutex);
			if (cef_global::activePopups.size() < cef_global::MAX_CONCURRENT_POPUPS) {
				printf("[KerneMsg] Creating popup window for process: %ws (slot available: %d/%d)\n", 
					Path.c_str(), (int)cef_global::activePopups.size(), cef_global::MAX_CONCURRENT_POPUPS);
				break;  // 有空槽，可以创建
			}
			printf("[KerneMsg] Maximum concurrent popups (%d) reached, waiting for available slot...\n", 
				cef_global::MAX_CONCURRENT_POPUPS);
			// 释放锁后等待
			Sleep(1000);  // 等待1秒后重试
		}
		
		// 创建弹窗
		bool popupCreated = cef_global::MainCefApp->SafeCreatePopUpWindow(index, Path, L"superduck", L"Very Dangerous");
		if (!popupCreated) {
			printf("[KerneMsg] Failed to create popup window for eventId=%d\n", index);
			CloseHandle(eventHandle);
			theLock.lock();
			eventIdMap.erase(index);
			theLock.unlock();
			return false;  // 创建失败，允许进程
		}

		const auto singleStatus =WaitForSingleObjectEx(eventHandle, 40 * 10000, false); //�ȴ�
		printf("[KerneMsg] WaitForSingleObjectEx result: 0x%X\n", singleStatus);
		if (singleStatus == WAIT_TIMEOUT) {
			printf("=== [TIMEOUT] ===\n");
			printf("[KerneMsg] Wait timeout (40s), allowing process by default\n");
			printf("=== [TIMEOUT RESULT: ALLOW] ===\n");
			
			// 【修复内存泄漏】超时时也需要清理eventIdMap和弹窗
			CloseHandle(eventHandle);
			theLock.lock();
			eventIdMap.erase(index);
			theLock.unlock();
			cef_global::AsyncCleanupPopup(index);  // 使用异步清理，避免卡顿
			printf("[KerneMsg] Timeout cleanup: scheduled async removal of eventId %d\n", index);
			return false;
		}
		else if (singleStatus == WAIT_OBJECT_0) {
			printf("=== [USER RESPONSE RECEIVED] ===\n");
			printf("[KerneMsg] Event signaled successfully, user made a choice\n");
			printf("[KerneMsg] Final decision from UI: %s\n", isNeedBlock ? "BLOCK" : "ALLOW");
		}
		else {
			// 处理其他错误状态 (WAIT_FAILED等)
			printf("=== [WAIT ERROR] ===\n");
			printf("[KerneMsg] WaitForSingleObjectEx failed with status: 0x%X, allowing process by default\n", singleStatus);
			CloseHandle(eventHandle);
			theLock.lock();
			eventIdMap.erase(index);
			theLock.unlock();
			cef_global::AsyncCleanupPopup(index);  // 使用异步清理，避免卡顿
			printf("[KerneMsg] Error cleanup: scheduled async removal of eventId %d\n", index);
			return false;
		}
		CloseHandle(eventHandle);
		printf("destory event index: %d \n", index);

		theLock.lock();
		eventIdMap.erase(index);
		theLock.unlock();
		printf("=== [FINAL DECISION] ===\n");
		printf("[KerneMsg] Final decision for process %ws: %s\n", Path.c_str(), isNeedBlock ? "BLOCK" : "ALLOW");
		printf("=== [RETURNING TO DRIVER] ===\n");
		return isNeedBlock;
	}


	auto HandleCefPopUpMsg(int eventId, bool isNeedBlock) -> void {
		printf("==========================================\n");
		printf("=== [用户决策接收] HandleCefPopUpMsg ===\n");
		printf("=== [INPUT PARAMETERS] ===\n");
		printf("[HandleCefPopUpMsg] *** eventId = %d ***\n", eventId);
		printf("[HandleCefPopUpMsg] *** isNeedBlock = %s ***\n", isNeedBlock ? "TRUE (BLOCK)" : "FALSE (ALLOW)");
		printf("==========================================\n");

		// 【修复竞态条件】添加锁保护对eventIdMap的访问
		theLock.lock();
		if (eventIdMap.find(eventId) == eventIdMap.end()) {
			printf("[错误] EventId %d 未找到!\n", eventId);
			theLock.unlock();
			return;
		}
		auto [eventHandle, blockThis] = eventIdMap[eventId];
		
		printf("=== [SETTING DECISION] ===\n");
		printf("[HandleCefPopUpMsg] *** BEFORE: *blockThis = %s ***\n", *blockThis ? "TRUE" : "FALSE");
		*blockThis = isNeedBlock;
		printf("[HandleCefPopUpMsg] *** AFTER:  *blockThis = %s ***\n", *blockThis ? "TRUE" : "FALSE");
		printf("=== [SIGNALING EVENT] ===\n");
		
		SetEvent(eventHandle);
		theLock.unlock();
		
		printf("=== [决策已发送] ===\n");
		printf("==========================================\n");
	}


	auto KernMsgRoute(void* ctx) -> void {
		static const auto bufferSize = 0x4000 + sizeof(FILTER_MESSAGE_HEADER);
		
		if (minifilterHandle == INVALID_HANDLE_VALUE) {
			return;
		}
		
		void* rawBuffer = malloc(bufferSize);
		if (rawBuffer == nullptr) {
			return;
		}
		memset(rawBuffer, 0x0, bufferSize);
		
		while (true) {
			memset(rawBuffer, 0x0, bufferSize);
			auto result = FilterGetMessage(minifilterHandle, (PFILTER_MESSAGE_HEADER)rawBuffer, bufferSize, NULL);
			if (result != S_OK) {
				break;
			}
			auto messageHeader = (PFILTER_MESSAGE_HEADER)rawBuffer;
			auto buffer = (void*)((uint64_t)rawBuffer + sizeof(FILTER_MESSAGE_HEADER));
			auto packetHeader = reinterpret_cast<_MsgClientPacket*>(buffer);
			
			if (packetHeader->magicNum != 0x1337) {
				continue;
			}
			switch (packetHeader->type)
			{
			case _ClientMsgType::kCreateProcess: {
				auto createProcess = reinterpret_cast<_MsgCreateProcess*>(buffer);

				auto fullPath = createProcess->path;
				auto fullCommandLine = ((wchar_t*)((uint64_t)createProcess + createProcess->commandOffset));

				printf("[KernMsgRoute] Processing create process request for: %ws\n", fullPath);
				printf("[KernMsgRoute] *** CALLING OnProcessCreate ***\n");
				bool blockDecision = OnProcessCreate(fullPath, fullCommandLine);
				printf("[KernMsgRoute] *** OnProcessCreate RETURNED: %s ***\n", blockDecision ? "BLOCK" : "ALLOW");
				printf("[KernMsgRoute] User decision received: %s\n", blockDecision ? "BLOCK" : "ALLOW");
				
				_Msg_CreateProcess_R3Reply theReply = { 0 };
				theReply.replyHeader.MessageId = messageHeader->MessageId;
				theReply.isNeedBlock = blockDecision;
				
				printf("==========================================\n");
				printf("=== [SENDING REPLY TO DRIVER] ===\n");
				printf("[KernMsgRoute] *** theReply.isNeedBlock = %s ***\n", blockDecision ? "TRUE (BLOCK)" : "FALSE (ALLOW)");
				printf("[KernMsgRoute] *** MessageId = 0x%llX ***\n", (unsigned long long)messageHeader->MessageId);
				printf("[KernMsgRoute] *** Reply size = %d ***\n", (int)sizeof(theReply));
				printf("==========================================\n");
				
				printf("[KernMsgRoute] Sending reply to driver: %s\n", blockDecision ? "BLOCK" : "ALLOW");
				auto replyResult = FilterReplyMessage(minifilterHandle,
					(PFILTER_REPLY_HEADER)&theReply,
					sizeof(theReply));
				
				if (replyResult != S_OK) {
					printf("[KernMsgRoute] FilterReplyMessage failed: 0x%X, retrying...\n", replyResult);
					// 重试一次
					Sleep(10);
					FilterReplyMessage(minifilterHandle,
						(PFILTER_REPLY_HEADER)&theReply,
						sizeof(theReply));
				}
				printf("[KernMsgRoute] Reply sent successfully for eventId\n");

			}break;
			case _ClientMsgType::kWriteFile:
			case _ClientMsgType::kReadFile: {
				auto fileEvent = reinterpret_cast<_MsgFileRw*>(buffer);

				auto fullPath = fileEvent->path;
				if (fileEvent->packetHeader.type == _ClientMsgType::kReadFile) {
					// printf("process %d readfile: %ws \n", fileEvent->pid, fullPath);
				}
				else {
					// printf("process %d writefile: %ws \n", fileEvent->pid, fullPath);
				}
			}break;
			default:
				break;
			}
		}
		free(rawBuffer);
	}
	auto WaitKernelMsg() -> void {

		KernMsgRoute(nullptr);
	}

	auto Init() -> bool {
		bool isConnect = false;
		HRESULT apiResult = S_OK;
		printf("[KerneMsg] ========== R3 CONNECTION ATTEMPT ==========\n");
		printf("[KerneMsg] Attempting to connect to minifilter port: %ws\n", Ring3CommPort);
		printf("[KerneMsg] Current thread ID: %lu\n", GetCurrentThreadId());
		printf("[KerneMsg] Process ID: %lu\n", GetCurrentProcessId());
		do
		{
			apiResult = FilterConnectCommunicationPort(Ring3CommPort, 0, NULL, 0, NULL, &minifilterHandle);
			printf("[KerneMsg] FilterConnectCommunicationPort result: 0x%X\n", apiResult);
			if (IS_ERROR(apiResult)) {
				printf("[KerneMsg] Failed to connect to minifilter, error: 0x%X\n", apiResult);
				printf("[KerneMsg] Common causes:\n");
				printf("[KerneMsg] - Driver not loaded or stopped\n");
				printf("[KerneMsg] - Communication port not created\n");
				printf("[KerneMsg] - Insufficient privileges (run as admin)\n");
				break;
			}
			printf("[KerneMsg] ========== CONNECTION SUCCESS ==========\n");
			printf("[KerneMsg] minifilterHandle: %p\n", minifilterHandle);
			printf("connect minifilter success \n");
			isConnect = true;
		} while (false);
		if (isConnect == false) {
			printf("[KerneMsg] ========== CONNECTION FAILED ==========\n");
			printf("connect minifilter failed \n");
		}
		return isConnect;
	}

	// 测试多弹窗功能
	auto TestMultiplePopups(int count) -> void {
		printf("[KerneMsg] ========== TESTING MULTIPLE POPUPS ==========\n");
		printf("[KerneMsg] Creating %d test popups...\n", count);
		
		// 模拟多个进程名称和路径
		std::vector<std::pair<std::wstring, std::wstring>> testProcesses = {
			{L"C:\\Windows\\System32\\notepad.exe", L"Notepad Program"},
			{L"C:\\Program Files\\TestApp\\malware.exe", L"Suspicious Program"},
			{L"C:\\Users\\Public\\virus.exe", L"Virus Program"},
			{L"C:\\Temp\\ransomware.exe", L"Ransomware"},
			{L"C:\\Downloads\\trojan.exe", L"Trojan Horse"},
			{L"C:\\System\\backdoor.exe", L"Backdoor Program"},
			{L"C:\\Apps\\keylogger.exe", L"Keylogger"},
			{L"C:\\Tools\\rootkit.exe", L"Rootkit Program"}
		};
		
		// 创建线程来模拟多个进程创建事件
		std::vector<std::thread> testThreads;
		
		for (int i = 0; i < count && i < (int)testProcesses.size(); i++) {
			testThreads.emplace_back([i, &testProcesses]() {
				// 每个线程延迟不同时间启动，模拟真实场景
				Sleep(i * 500); // 每个弹窗间隔0.5秒
				
				std::wstring processPath = testProcesses[i].first;
				std::wstring processName = testProcesses[i].second;
				std::wstring commandLine = processPath + L" /test";
				
				printf("[KerneMsg] Test Thread %d: Creating popup for %ws\n", i+1, processPath.c_str());
				
				bool result = OnProcessCreate(processPath, commandLine);
				
				printf("[KerneMsg] Test Thread %d: User decision for %ws: %s\n", 
					i+1, processPath.c_str(), result ? "BLOCK" : "ALLOW");
			});
		}
		
		printf("[KerneMsg] All test threads created, waiting for completion...\n");
		
		// 等待所有测试线程完成
		for (auto& thread : testThreads) {
			if (thread.joinable()) {
				thread.join();
			}
		}
		
		printf("[KerneMsg] ========== MULTIPLE POPUPS TEST COMPLETED ==========\n");
	}

};