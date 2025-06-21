#include "kernelmsg.h"

namespace KernelMsg {
	size_t msgQueueLength = 0;
	ERESOURCE queueLock;
	uint32_t MaxMessageLength = PAGE_SIZE * 4;
	size_t MaxMsgQueueLength = 0x1000 * 5;
	LIST_ENTRY* msgQueue = nullptr;
	static LARGE_INTEGER MessageTimeout;
	bool LockInited = false;
	bool isDriverUninstall = false;

	auto PushMsgToQueue(void* msgBuffer, size_t msgSize) -> void {
		// 由于使用的资源锁,这个并不支持DPC级别
		PAGED_CODE();
		bool successInsert = false;
		_Ring3MsgQueue* Item = nullptr;
		ExAcquireResourceExclusiveLite(&queueLock, true);

		do {
			if (msgBuffer == nullptr || msgSize == 0 || isDriverUninstall) {
				break;
			}
			if (msgQueueLength > MaxMsgQueueLength) {
				DebugPrint("[%s] Drop Msg Because Buffer full\n", __func__);
				break;
			}
			if (msgSize > MaxMessageLength) {
				DebugPrint("[%s] Drop Msg Because Buffer Msg size was overflow %d \n",__func__, msgSize);
				break;
			}

			Item = reinterpret_cast<_Ring3MsgQueue*>(
				ExAllocatePoolWithTag(PagedPool, sizeof(_Ring3MsgQueue), Tools::poolTag));

			if (Item == nullptr) {
				break;
			}
			memset(Item, 0x0, sizeof(_Ring3MsgQueue));
			Item->buffer = static_cast<uint8_t*>(
				ExAllocatePoolWithTag(PagedPool, msgSize, Tools::poolTag));
			if (Item->buffer == nullptr) {
				ExFreePoolWithTag(Item, Tools::poolTag);
				break;
			}
			successInsert = true;
			memcpy(Item->buffer, msgBuffer, msgSize);
			Item->bufferSize = msgSize;
			InsertTailList(msgQueue, &Item->ListEntry);
			msgQueueLength++;
		} while (false);
		if (successInsert == false) {
			if (Item != nullptr) {
				if (Item->buffer != nullptr) {
					ExFreePoolWithTag(Item->buffer,Tools::poolTag);
					Item->buffer = nullptr;
				}
				ExFreePoolWithTag(Item, Tools::poolTag);
				Item = nullptr;
			}
		}
		ExReleaseResourceLite(&queueLock);
	}

	auto FreeMsg(_Ring3MsgQueue* msg) -> void {
		if (msg == nullptr) {
			return;
		}
		if (msg->buffer != nullptr) {
			ExFreePoolWithTag(msg->buffer, Tools::poolTag);
		}
		ExFreePoolWithTag(msg, Tools::poolTag);
	}
	auto GetMsgFromQueue() -> _Ring3MsgQueue* {
		if (msgQueueLength == 0) {
			return nullptr;
		}
		ExAcquireResourceExclusiveLite(&queueLock, true);
		PLIST_ENTRY entry = RemoveHeadList(msgQueue);
		_Ring3MsgQueue* node = CONTAINING_RECORD(entry, _Ring3MsgQueue, ListEntry);
		msgQueueLength--;
		ExReleaseResourceLite(&queueLock);

		return node;
	}
	//正确的做法是等信号量,客户端链接后发信号量,有日志后发信号量,直接SLEEP有点太癌症了,但是累了,就这样吧
	auto Sleep(LONG milliseconds) -> void {
		LARGE_INTEGER interval;
		interval.QuadPart = -(10000ll * milliseconds);

		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
	auto serverThread(void* ctx) -> void {
		HANDLE handle = reinterpret_cast<HANDLE>(ctx);
		_Ring3MsgQueue* msgBuffer = nullptr;
		do {

			if (isDriverUninstall || Minifilter::filterHandle == nullptr) {
				break;
			}
			if (Minifilter::ClientHandle == nullptr) {
				Sleep(1000 * 10);
				continue;
			}
			_Ring3MsgQueue* msgBuffer = GetMsgFromQueue();
			if (msgBuffer == nullptr) {
				Sleep(100);
				continue;
			}
			FltSendMessage(Minifilter::filterHandle,
				&Minifilter::ClientHandle,
				msgBuffer->buffer,
				msgBuffer->bufferSize, NULL, NULL, NULL);
			FreeMsg(msgBuffer);
			Sleep(100);

		} while (true);
		if (msgBuffer) {
			FreeMsg(msgBuffer);
		}
		DebugPrint("[%s] Server thread exit! \n", __func__);
	}



	HANDLE ServerThreadHandle;
	auto Init() -> bool {
		// 30秒超时
		MessageTimeout.QuadPart = Int32x32To64(30, -10 * 1000 * 1000);
		auto ntstatus = ExInitializeResourceLite(&queueLock);
		if (NT_SUCCESS(ntstatus) == false) {
			return false;
		}
		LockInited = true;
		msgQueue = static_cast<LIST_ENTRY*>(ExAllocatePoolWithTag(PagedPool, sizeof(LIST_ENTRY), Tools::poolTag));
		if (msgQueue == nullptr) {
			DebugPrint("msgQuque is NULL");
			return false;
		}
		memset(msgQueue, 0x0, sizeof(LIST_ENTRY));

		InitializeListHead(msgQueue);
		bool threadSuccesInit = false;
		do {
			auto ntStatus = PsCreateSystemThread(
				&ServerThreadHandle, THREAD_ALL_ACCESS, NULL,
				NtCurrentProcess(), NULL, serverThread, nullptr);
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			}
			threadSuccesInit = true;
			DebugPrint("[%s] Create RPC Server Success \n", __func__);
		} while (false);

		return threadSuccesInit;
	}

	auto Uninstall() -> void {
		isDriverUninstall = true;
		DebugPrint("[%s] CleanUpQueue Length: %d \n", __func__, msgQueueLength);
		if (LockInited == false) {
			return;
		}
		PVOID threadObject;
		if (ServerThreadHandle != nullptr) {
			if (NT_SUCCESS(ObReferenceObjectByHandle(
				ServerThreadHandle, THREAD_ALL_ACCESS, NULL, KernelMode,
				&threadObject, NULL))) {
				KeWaitForSingleObject(threadObject, Executive, KernelMode, FALSE,
					NULL);
				ObDereferenceObject(threadObject);
			}
			ZwClose(ServerThreadHandle);
			ServerThreadHandle = nullptr;
		}
		ExAcquireResourceExclusiveLite(&queueLock, true);

		if (msgQueue != nullptr) {
			while (IsListEmpty(msgQueue) == false) {
				PLIST_ENTRY entry = RemoveHeadList(msgQueue);
				_Ring3MsgQueue* node =
					CONTAINING_RECORD(entry, _Ring3MsgQueue, ListEntry);
				FreeMsg(node);
			}
			//woops, we still missing something
		}
		ExReleaseResourceLite(&queueLock);

		msgQueueLength = 0;
		ExDeleteResourceLite(&queueLock);
	}

	auto SendCreateProcessEvent(_MsgCreateProcess* Msg, size_t MsgSize) -> bool {
		bool blockProcess = false;
		if (MsgSize > MaxMessageLength) {
			// bugged, fix me
			__debugbreak();
			return blockProcess;
		}
		// 这里并不安全.客户端应该做计数引用
		if (Minifilter::filterHandle == nullptr || Minifilter::ClientHandle == nullptr || isDriverUninstall) {
			return blockProcess;
		}

		_Msg_CreateProcess_R3Reply theReply = {};
		uint32_t replySize = 0;

		auto ntStatus =
			FltSendMessage(Minifilter::filterHandle, &Minifilter::ClientHandle, Msg,
				MsgSize, &theReply, (PULONG)&replySize, &MessageTimeout);
		if (NT_SUCCESS(ntStatus)) {
			blockProcess = theReply.isNeedBlock;
		}

		return blockProcess;
	}
}


	
