#include "kernel_msg.h"


namespace KerneMsg {
	int currentIndex = 0;;
	std::map<int, std::tuple<void*, bool*>> eventIdMap;
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
	struct _Msg_CreateProcess_R3Reply {
		FILTER_REPLY_HEADER replyHeader;
		bool isNeedBlock;
	};

	static const auto Ring3CommPort = L"\\DuckGuardCommPort";
	HANDLE minifilterHandle = INVALID_HANDLE_VALUE;

	auto OnProcessCreate(std::wstring Path, std::wstring CommandLine) -> bool {
		if (Path.find(L"duck_guard_win_app") != -1) {
			return false;
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
		printf("create event index: %d \n", index);
		cef_global::MainCefApp->SafeCreatePopUpWindow(index, Path, L"superduck", L"非常危险");

		const auto singleStatus =
			WaitForSingleObjectEx(eventHandle, 40 * 10000, false);
		if (singleStatus == WAIT_TIMEOUT) {
			return false;
		}
		CloseHandle(eventHandle);
		printf("destory event index: %d \n", index);

		theLock.lock();
		eventIdMap.erase(index);
		theLock.unlock();
		return isNeedBlock;
	}


	auto HandleCefPopUpMsg(int eventId, bool isNeedBlock) -> void {
		printf("handle event index: %d \n", eventId);

		if (eventIdMap.find(eventId) == eventIdMap.end()) {
			//impossble
			__debugbreak();
			return;
		}
		auto [eventHandle, blockThis] = eventIdMap[eventId];
		*blockThis = isNeedBlock;
		SetEvent(eventHandle);
	}


	auto KernMsgRoute(void* ctx) -> void {
		static const auto bufferSize = 0x4000 + sizeof(FILTER_MESSAGE_HEADER);
		void* rawBuffer = malloc(bufferSize);
		if (rawBuffer == nullptr) {
			DebugBreak();
			return;
		}
		memset(rawBuffer, 0x0, bufferSize);
		while (true) {
			//这里其实要IOCP的,算了,太复杂了
			memset(rawBuffer, 0x0, bufferSize);
			auto result = FilterGetMessage(minifilterHandle, (PFILTER_MESSAGE_HEADER)rawBuffer, bufferSize, NULL);
			if (result != S_OK) {
				break;
			}
			auto messageHeader = (PFILTER_MESSAGE_HEADER)rawBuffer;
			auto buffer = (void*)((uint64_t)rawBuffer + sizeof(FILTER_MESSAGE_HEADER));
			auto packetHeader = reinterpret_cast<_MsgClientPacket*>(buffer);
			if (packetHeader->magicNum != 0x1337) {
				DebugBreak();
				continue;
			}
			switch (packetHeader->type)
			{
			case _ClientMsgType::kCreateProcess: {
				auto createProcess = reinterpret_cast<_MsgCreateProcess*>(buffer);

				auto fullPath = createProcess->path;
				auto fullCommandLine = ((wchar_t*)((uint64_t)createProcess + createProcess->commandOffset));
				printf("create process at %ws command line: %ws \n", fullPath, fullCommandLine);

				_Msg_CreateProcess_R3Reply theReply = { 0 };
				theReply.replyHeader.MessageId = messageHeader->MessageId;
				theReply.isNeedBlock = OnProcessCreate(fullPath, fullCommandLine);
				FilterReplyMessage(minifilterHandle,
					(PFILTER_REPLY_HEADER)&theReply,
					sizeof(theReply));

			}break;
			case _ClientMsgType::kWriteFile:
			case _ClientMsgType::kReadFile: {
				auto fileEvent = reinterpret_cast<_MsgFileRw*>(buffer);

				auto fullPath = fileEvent->path;
				if (fileEvent->packetHeader.type == _ClientMsgType::kReadFile) {
					printf("process %d readfile: %ws \n", fileEvent->pid, fullPath);
				}
				else {
					printf("process %d writefile: %ws \n", fileEvent->pid, fullPath);
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
		do
		{
			apiResult = FilterConnectCommunicationPort(Ring3CommPort, 0, NULL, 0, NULL, &minifilterHandle);
			if (IS_ERROR(apiResult)) {
				break;
			}
			printf("connect minifilter success \n");
			isConnect = true;
		} while (false);
		if (isConnect == false) {
			printf("connect minifilter failed \n");
		}
		return isConnect;
	}

};