#include <iostream>
#include <Windows.h>
#include <fltuser.h>
#include <vector>
#include <share.h>
#pragma comment(lib, "fltLib.lib")
extern "C" {
	HRESULT
		WINAPI
		FilterConnectCommunicationPort(
			_In_ LPCWSTR lpPortName,
			_In_ DWORD dwOptions,
			_In_reads_bytes_opt_(wSizeOfContext) LPCVOID lpContext,
			_In_ WORD wSizeOfContext,
			_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
			_Outptr_ HANDLE* hPort
		);
	HRESULT
		WINAPI
		FilterSendMessage(
			_In_ HANDLE hPort,
			_In_reads_bytes_(dwInBufferSize) LPVOID lpInBuffer,
			_In_ DWORD dwInBufferSize,
			_Out_writes_bytes_to_opt_(dwOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
			_In_ DWORD dwOutBufferSize,
			_Out_ LPDWORD lpBytesReturned
		);;
};

namespace Tools {
	auto EasyCreateThread(void* pFunctionAddress, void* pParams) -> HANDLE {
		return CreateThread(
			NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(pFunctionAddress),
			static_cast<LPVOID>(pParams), NULL, NULL);
	}
};
namespace KerneMsg {
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
				theReply.isNeedBlock = false;
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

int main()
{
	if (KerneMsg::Init()) {
		KerneMsg::WaitKernelMsg();
		return 0;
	}
	printf("Check the Ring3CommPort Name and Administrator privileges to Start \n");
	system("puase");
	return 0;
}