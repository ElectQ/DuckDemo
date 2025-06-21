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
		KeEnterCriticalRegion(); //进入临界区 禁用APC调用资源保证锁的稳定性
		ExAcquireResourceExclusiveLite(&queueLock, true);  //获取当前进程资源

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
		ExReleaseResourceLite(&queueLock);  //释放这个锁
		KeLeaveCriticalRegion(); //离开临界区 重新允许APC调用资源
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
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(&queueLock, true);
		PLIST_ENTRY entry = RemoveHeadList(msgQueue);
		_Ring3MsgQueue* node = CONTAINING_RECORD(entry, _Ring3MsgQueue, ListEntry);
		msgQueueLength--;
		ExReleaseResourceLite(&queueLock);
		KeLeaveCriticalRegion();

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
			msgBuffer = nullptr;

			if (isDriverUninstall || Minifilter::filterHandle == nullptr) {
				if (msgBuffer != nullptr) { // 确保在退出前释放当前消息
					FreeMsg(msgBuffer);
					msgBuffer = nullptr;
				}
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

			if (isDriverUninstall || Minifilter::filterHandle == nullptr || Minifilter::ClientHandle == nullptr) {
				FreeMsg(msgBuffer); // 如果要退出，释放消息
				msgBuffer = nullptr;
				break; // 立即退出
			}

			FltSendMessage(Minifilter::filterHandle,
				&Minifilter::ClientHandle,
				msgBuffer->buffer,
				msgBuffer->bufferSize, NULL, NULL, &KernelMsg::MessageTimeout);
			FreeMsg(msgBuffer);
			Sleep(100);

		} while (true);
		FreeMsg(msgBuffer);
		msgBuffer = nullptr; // 及时清空，避免在下次循环开始前错误引用
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
			auto ntStatus = PsCreateSystemThread(&ServerThreadHandle, THREAD_ALL_ACCESS, NULL,NtCurrentProcess(), NULL, serverThread, nullptr);
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			}
			threadSuccesInit = true;
			DebugPrint("[%s] Create RPC Server Success \n", __func__);
		} while (false);

		return threadSuccesInit;
	}

	auto Uninstall() -> void {
		DebugPrint("[KernelMsg] Uninstall: Entering.\n"); // 
		isDriverUninstall = true; // 放置在最前面，尽快通知其他线程
		DebugPrint("[KernelMsg] Uninstall: isDriverUninstall set to true. CleanUpQueue Length: %d\n", msgQueueLength); // 改进

		if (LockInited == false) {
			DebugPrint("[KernelMsg] Uninstall: Lock not initialized, exiting.\n"); // 
			return;
		}

		PVOID threadObject = nullptr; // 初始化为 nullptr
		if (ServerThreadHandle != nullptr) {
			DebugPrint("[KernelMsg] Uninstall: Attempting to reference serverThread handle %p.\n", ServerThreadHandle); 
			if (NT_SUCCESS(ObReferenceObjectByHandle(ServerThreadHandle, THREAD_ALL_ACCESS, NULL, KernelMode,&threadObject, NULL))) {
				DebugPrint("[KernelMsg] Uninstall: Reference acquired. Waiting for serverThread to terminate.\n"); 
				KeWaitForSingleObject(threadObject, Executive, KernelMode, FALSE, NULL);  //serverThread 线程函数已经返回，内核可能还需要一些时间来完成线程对象的清理工作
				DebugPrint("[KernelMsg] Uninstall: serverThread terminated.\n"); // 
				ObDereferenceObject(threadObject);
				DebugPrint("[KernelMsg] Uninstall: Thread object dereferenced.\n"); // 
			}
			else {
				DebugPrint("[KernelMsg] Uninstall: Failed to reference serverThread object! Status: 0x%X\n", STATUS_UNSUCCESSFUL); 
			}
			ZwClose(ServerThreadHandle);
			DebugPrint("[KernelMsg] Uninstall: ServerThreadHandle %p closed.\n", ServerThreadHandle); 
			ServerThreadHandle = nullptr;
		}
		else {
			DebugPrint("[KernelMsg] Uninstall: ServerThreadHandle is NULL, no thread to wait for.\n"); 
		}

		DebugPrint("[KernelMsg] Uninstall: Entering critical region for queue cleanup.\n"); 
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(&queueLock, true);
		DebugPrint("[KernelMsg] Uninstall: queueLock acquired for cleanup.\n"); // 

		if (msgQueue != nullptr) {
			DebugPrint("[KernelMsg] Uninstall: Clearing message queue. Current length: %zu\n", msgQueueLength); // 
			while (IsListEmpty(msgQueue) == false) {
				PLIST_ENTRY entry = RemoveHeadList(msgQueue);
				_Ring3MsgQueue* node = CONTAINING_RECORD(entry, _Ring3MsgQueue, ListEntry);
				FreeMsg(node);
			}
			ExFreePoolWithTag(msgQueue, Tools::poolTag);
			msgQueue = nullptr; // 设置为nullptr
			DebugPrint("[KernelMsg] Uninstall: msgQueue freed.\n"); // 
		}
		else {
			DebugPrint("[KernelMsg] Uninstall: msgQueue is NULL, nothing to free.\n"); // 
		}

		ExReleaseResourceLite(&queueLock);
		DebugPrint("[KernelMsg] Uninstall: queueLock released after cleanup.\n"); // 

		msgQueueLength = 0;
		ExDeleteResourceLite(&queueLock);
		LockInited = false; // 重置标志
		DebugPrint("[KernelMsg] Uninstall: queueLock deleted.\n"); // 
		KeLeaveCriticalRegion();
		DebugPrint("[KernelMsg] Uninstall: Critical region left. Exiting.\n"); // 
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


	
