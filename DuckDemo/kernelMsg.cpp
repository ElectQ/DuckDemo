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
		// DebugPrint("[KernelMsg] PushMsgToQueue: msgSize=%lu, queueLength=%lu\n", (unsigned long)msgSize, (unsigned long)msgQueueLength);
		KeEnterCriticalRegion(); //进入临界区 禁用APC调用资源保证锁的稳定性
		ExAcquireResourceExclusiveLite(&queueLock, true);  //获取当前进程资源

		do {
			if (msgBuffer == nullptr || msgSize == 0 || isDriverUninstall) {
				break;
			}
			if (msgQueueLength > MaxMsgQueueLength) {
				DebugPrint("[KernelMsg] Drop Msg Because Buffer full, queueLength=%lu\n", (unsigned long)msgQueueLength);
				break;
			}
			if (msgSize > MaxMessageLength) {
				DebugPrint("[%s] Drop Msg Because Buffer Msg size was overflow %lu \n",__func__, (unsigned long)msgSize);
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
			// DebugPrint("[KernelMsg] Message added to queue successfully, new queueLength=%lu\n", (unsigned long)msgQueueLength);
		} while (false);
		if (successInsert == false) {
			DebugPrint("[KernelMsg] Failed to insert message to queue\n");
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
		DebugPrint("[KernelMsg] Server thread started\n");
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
				DebugPrint("[KernelMsg] Waiting for client connection...\n");
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

			// DebugPrint("[KernelMsg] Sending message to R3 client, bufferSize=%lu\n", (unsigned long)msgBuffer->bufferSize);
			auto sendResult = FltSendMessage(Minifilter::filterHandle,
				&Minifilter::ClientHandle,
				msgBuffer->buffer,
				msgBuffer->bufferSize, NULL, NULL, &KernelMsg::MessageTimeout);
			// DebugPrint("[KernelMsg] FltSendMessage result: 0x%X\n", sendResult);
			FreeMsg(msgBuffer);
			Sleep(100);

		} while (true);
		FreeMsg(msgBuffer);
		msgBuffer = nullptr; // 及时清空，避免在下次循环开始前错误引用
		DebugPrint("[%s] Server thread exit! \n", __func__);
	}



	HANDLE ServerThreadHandle;
	auto Init() -> bool {
		// 45秒超时 - 稍长于R3端的40秒，确保有足够时间响应
		MessageTimeout.QuadPart = Int32x32To64(45, -10 * 1000 * 1000);
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
		DebugPrint("[KernelMsg] Uninstall: isDriverUninstall set to true. CleanUpQueue Length: %lu\n", (unsigned long)msgQueueLength); // 改进

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
			DebugPrint("[KernelMsg] Uninstall: Clearing message queue. Current length: %lu\n", (unsigned long)msgQueueLength); // 
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
		// DebugPrint("[KernelMsg] SendCreateProcessEvent: MsgSize=%lu, MaxMessageLength=%d\n", (unsigned long)MsgSize, MaxMessageLength);
		if (MsgSize > MaxMessageLength) {
			DebugPrint("[KernelMsg] Message size %lu exceeds maximum %d, allowing process\n", (unsigned long)MsgSize, MaxMessageLength);
			return blockProcess;
		}
		// 这里并不安全.客户端应该做计数引用
		if (Minifilter::filterHandle == nullptr || Minifilter::ClientHandle == nullptr || isDriverUninstall) {
			DebugPrint("[KernelMsg] Cannot send message: filterHandle=%p, ClientHandle=%p, isDriverUninstall=%s\n", 
				Minifilter::filterHandle, Minifilter::ClientHandle, isDriverUninstall ? "true" : "false");
			return blockProcess;
		}
	

		_Msg_CreateProcess_R3Reply theReply = {};
		uint32_t replySize = sizeof(_Msg_CreateProcess_R3Reply);

		DebugPrint("[KernelMsg] *** BEFORE SEND: Expected replySize = %d ***\n", replySize);
		DebugPrint("[KernelMsg] *** FILTER_REPLY_HEADER size = %d ***\n", (int)sizeof(FILTER_REPLY_HEADER));
		DebugPrint("[KernelMsg] *** bool size = %d ***\n", (int)sizeof(bool));
		DebugPrint("[KernelMsg] *** Total struct size = %d ***\n", (int)sizeof(_Msg_CreateProcess_R3Reply));

		DebugPrint("[KernelMsg] Sending synchronous message to R3 and waiting for reply...\n");
		auto ntStatus =
			FltSendMessage(Minifilter::filterHandle, &Minifilter::ClientHandle, Msg,  //阻塞等待
				MsgSize, &theReply, (PULONG)&replySize, &MessageTimeout);
		DebugPrint("[KernelMsg] FltSendMessage returned: 0x%X, replySize=%d\n", ntStatus, replySize);
		if (NT_SUCCESS(ntStatus) && ntStatus != STATUS_TIMEOUT) {
			DebugPrint("[KernelMsg] *** REPLY STRUCTURE ANALYSIS ***\n");
			DebugPrint("[KernelMsg] *** theReply.replyHeader.MessageId = 0x%llX ***\n", (unsigned long long)theReply.replyHeader.MessageId);
			DebugPrint("[KernelMsg] *** Raw memory at isNeedBlock offset: ***\n");
			
			// 直接访问isNeedBlock字段的内存
			unsigned char* replyBytes = (unsigned char*)&theReply;
			for (int i = 0; i < sizeof(_Msg_CreateProcess_R3Reply); i++) {
				DebugPrint("[KernelMsg] Byte[%d] = 0x%02X\n", i, replyBytes[i]);
			}
			
			// 【修复】由于只接收到8字节，而FILTER_REPLY_HEADER可能有16字节
			// 实际的isNeedBlock数据在接收到的数据的最后位置
			bool actualDecision = false;
			if (replySize >= sizeof(FILTER_REPLY_HEADER) + sizeof(bool)) {
				// 正常情况：完整接收
				actualDecision = theReply.isNeedBlock;
				DebugPrint("[KernelMsg] *** Using theReply.isNeedBlock (normal case) ***\n");
			} else {
				// 特殊情况：只接收到部分数据，从Byte[0]读取（这是R3实际发送的决策）
				actualDecision = (replyBytes[0] != 0);
				DebugPrint("[KernelMsg] *** Using Byte[0] as decision (replySize=%d < expected) ***\n", replySize);
			}
			
			blockProcess = actualDecision;
			DebugPrint("=== [DRIVER DECISION RECEIVED] ===\n");
			DebugPrint("[KernelMsg] *** R3 DECISION: %s PROCESS ***\n", 
				blockProcess ? "BLOCK" : "ALLOW");
			DebugPrint("[KernelMsg] *** Actual decision from Byte[0]: %s ***\n", 
				(replyBytes[0] != 0) ? "TRUE" : "FALSE");
			DebugPrint("[KernelMsg] *** theReply.isNeedBlock: %s ***\n", 
				theReply.isNeedBlock ? "TRUE" : "FALSE");
			DebugPrint("=== [DRIVER WILL %s PROCESS] ===\n", 
				blockProcess ? "BLOCK" : "ALLOW");
		}
		else {
			if (ntStatus == STATUS_TIMEOUT || ntStatus == 0x102) {
				DebugPrint("=== [DRIVER TIMEOUT] ===\n");
				DebugPrint("[KernelMsg] *** R3 response timeout (0x%X) ***\n", ntStatus);
				DebugPrint("=== [DRIVER WILL ALLOW PROCESS BY DEFAULT] ===\n");
			} else {
				DebugPrint("=== [DRIVER COMMUNICATION FAILED] ===\n");
				DebugPrint("[KernelMsg] *** Failed to get reply from R3 (0x%X) ***\n", ntStatus);
				DebugPrint("=== [DRIVER WILL ALLOW PROCESS BY DEFAULT] ===\n");
			}
		}

		DebugPrint("=== [FINAL DRIVER DECISION] ===\n");
		DebugPrint("[KernelMsg] *** RETURNING TO CALLBACK: %s ***\n", 
			blockProcess ? "BLOCK PROCESS" : "ALLOW PROCESS");
		DebugPrint("=== [END DRIVER DECISION] ===\n");

		return blockProcess;
	}
}


	
