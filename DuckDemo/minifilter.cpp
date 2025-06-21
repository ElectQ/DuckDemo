#include "minifilter.h"

namespace Minifilter {

	PFLT_PORT CommPortHandle;
	PFLT_PORT ClientHandle;
	ERESOURCE ClientLock;
	PFLT_FILTER filterHandle;
	bool LockInited = false;

	auto PostCleanUpOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS {
		_Minifilter_Stream_Context* streamCtx = nullptr;
		
		do
		{
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			const auto pid = PsGetCurrentProcessId();
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			if (NT_SUCCESS(FltGetStreamHandleContext(
				FltObjects->Instance, FltObjects->FileObject,
				reinterpret_cast<PFLT_CONTEXT*>(&streamCtx))) == false) {
				break;
			}
			if (streamCtx == nullptr) {
				break;
			}
			if (streamCtx->accessType == _FileAccessType::kNomal && (streamCtx->hasReadFile || streamCtx->hasWriteFile)) {

				size_t msgSize = sizeof(_MsgFileRw) + streamCtx->rawPath->Length;
				_MsgFileRw* msg = reinterpret_cast<_MsgFileRw*>(
					ExAllocatePoolWithTag(PagedPool, msgSize, Tools::poolTag));
				do
				{
					if (msg == nullptr) {
						break;
					}
					memset(msg, 0x0, msgSize);
					msg->packetHeader.magicNum = 0x1337;
					msg->packetHeader.type = streamCtx->hasReadFile ? _ClientMsgType::kReadFile : _ClientMsgType::kWriteFile;
					msg->pid = reinterpret_cast<uint32_t>(streamCtx->pid);
					memcpy(msg->path, streamCtx->rawPath->Buffer, streamCtx->rawPath->Length);
					KernelMsg::PushMsgToQueue(msg, msgSize);  //推送信息进队列慢慢发送
				} while (false);
				if (msg) {
					ExFreePoolWithTag(msg, Tools::poolTag);
					msg = nullptr;
				}
				}

			// DebugPrint("process %d write file: %wZ \n", pid, streamCtx->rawPath);

			} while (false);

			if (streamCtx) {
				FltReleaseContext(reinterpret_cast<PFLT_CONTEXT>(streamCtx));  //不安全的转换
			}
			return FLT_POSTOP_FINISHED_PROCESSING;
		
	}

	auto PostReadOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS {
		_Minifilter_Stream_Context* streamCtx = nullptr;
		do
		{
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			const auto pid = PsGetCurrentProcessId();
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			if (NT_SUCCESS(FltGetStreamHandleContext(
				FltObjects->Instance, FltObjects->FileObject,
				reinterpret_cast<PFLT_CONTEXT*>(&streamCtx))) == false) {
				break;
			}
			if (streamCtx == nullptr) {
				break;
			}
			streamCtx->hasReadFile = true;
			//DebugPrint("process %d read file: %wZ \n", pid, streamCtx->rawPath);
		} while (FALSE);
		if (streamCtx) {
			FltReleaseContext(reinterpret_cast<PFLT_CONTEXT>(streamCtx));  //不安全的转换
		}
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	auto PostWriteOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS{
		_Minifilter_Stream_Context* streamCtx = nullptr;
		do
		{
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			const auto pid = PsGetCurrentProcessId();
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			if (NT_SUCCESS(FltGetStreamHandleContext(
				FltObjects->Instance, FltObjects->FileObject,
				reinterpret_cast<PFLT_CONTEXT*>(&streamCtx))) == false) {
				break;
			}
			if (streamCtx == nullptr) {
				break;
			}
			streamCtx->hasWriteFile = true;
			//DebugPrint("process %d write file: %wZ \n", pid, streamCtx->rawPath);
		} while (FALSE);
		if (streamCtx) {
			FltReleaseContext(reinterpret_cast<PFLT_CONTEXT>(streamCtx));  //不安全的转换
		}
		return FLT_POSTOP_FINISHED_PROCESSING;
	
	}


	auto freeStreamCtx(_Minifilter_Stream_Context* streamCtx) -> void {
		if (streamCtx->rawPath) {
			Tools::FreeCopiedString(streamCtx->rawPath);
			streamCtx->rawPath = nullptr;
		}
	}

	auto createStreamCtx(PCFLT_RELATED_OBJECTS fltObjects, PFLT_CALLBACK_DATA Data, bool isInPreCallbacks = false)->_Minifilter_Stream_Context* {
		_Minifilter_Stream_Context* streamCtx = nullptr;
		PFLT_CONTEXT fltCtxPtr = nullptr;
		PFLT_FILE_NAME_INFORMATION nameInfo = nullptr;
		bool status = false;
		do
		{
			auto ntStatus = FltAllocateContext(fltObjects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(_Minifilter_Stream_Context), NonPagedPoolNx, &fltCtxPtr);  //为指定的上下文类型分配一个上下文结构
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			} 
			streamCtx = static_cast<_Minifilter_Stream_Context*>(fltCtxPtr);
			memset(streamCtx, 0, sizeof(_Minifilter_Stream_Context));  //内核里面你申请的内存不会自动给你清零




			if (NT_SUCCESS(FltGetFileNameInformation(
				Data,
				FLT_FILE_NAME_NORMALIZED |
				FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,  //查询 Filter Manager 的名称缓存以获取文件名信息。如果名称未在缓存中找到，并且当前安全，FltGetFileNameInformation 会查询文件系统以获取文件名信息并将结果缓存
				&nameInfo)) == false) {
				break;
			}
			
		

			status = Tools::CopyUnicodeStringWithAllocPagedMemory(&streamCtx->rawPath, &nameInfo->Name);   //复制字符串内容

			streamCtx->pid = PsGetCurrentProcessId();

			if (RtlPrefixUnicodeString(&accessNamedPipe, streamCtx->rawPath, true)) {
				streamCtx->accessType = _FileAccessType::kPipe;
			}
			else {
				streamCtx->accessType = _FileAccessType::kNomal;
			}

			ntStatus = FltSetStreamHandleContext(fltObjects->Instance, fltObjects->FileObject,FLT_SET_CONTEXT_REPLACE_IF_EXISTS, fltCtxPtr, nullptr);  //将上下文结构取代当前句柄的



			if (NT_SUCCESS(status) == FALSE) {
				return FALSE;
			}

			//DebugPrint("post create file name: %wZ \n", nameInfo->Name);
			status = true;

		} while (false);
		if (status == false) {
			freeStreamCtx(streamCtx);
			streamCtx = nullptr;
		}//这里因为只调用一次，所以后面全部释放
		if (fltCtxPtr) {
			FltReleaseContext(fltCtxPtr);
		}
		if (nameInfo) {
			FltReleaseFileNameInformation(nameInfo);
		}
		return streamCtx;
	}

	auto PostCreateOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)-> FLT_POSTOP_CALLBACK_STATUS {
		
		_Minifilter_Stream_Context* streamCtx = nullptr;
		do
		{
			if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {
				break;
			}
			if (NT_SUCCESS(Data->IoStatus.Status) == false ||
				(Data->IoStatus.Status == STATUS_REPARSE)) {
				break;
			}
			if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
				break;
			}
			const auto pid = PsGetCurrentProcessId();
			streamCtx = createStreamCtx(FltObjects, Data, FALSE);
			if (streamCtx == nullptr) {
				break; // 或者返回其他适当的状态，不进行 DebugPrint
			}
			//DebugPrint("process %d create file name: %wZ \n",pid ,streamCtx->rawPath);
		} while (false);


		//DebugPrint("post create op \n");
		return FLT_POSTOP_FINISHED_PROCESSING;

	}


	auto PreCreateOperation(PFLT_CALLBACK_DATA Data,	PCFLT_RELATED_OBJECTS FltObjects,	PVOID* CompletionContext) -> FLT_PREOP_CALLBACK_STATUS {
		auto createStatus = FLT_PREOP_SUCCESS_NO_CALLBACK; // 已经成功处理了当前的预操作，并且不需要在 I/O 操作完成后再调用其后操作回调（Post-operation Callback）
		do { // 使用 do-while(false) 结构方便跳出
			// 1. 检查是否为打开分页文件操作
			if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE)) {
				break; // 如果是，则直接跳出，不做任何额外处理
			}

			// 2. 获取当前进程ID并检查
			const auto pid = PsGetCurrentProcessId();
			if ((ULONG)pid <= 4) { // 通常系统进程的PID较低（如System、Smss等）
				break; // 如果是系统关键进程，则直接跳出，不做额外处理
			}

			// 3. 检查是否为快速 I/O 操作
			if (FLT_IS_FASTIO_OPERATION(Data)) {
				break; // 如果是快速 I/O，通常意味着该操作不适合进行复杂处理，直接跳出
			}

			// 4. 再次检查是否为打开分页文件操作 (重复代码)
			// 这是一个重复的条件判断，与第1点完全相同
			if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE)) {
				break; // 如果是，则直接跳出
			}

			// 5. 检查是否为创建命名管道操作
			if (Data->Iopb->MajorFunction == IRP_MJ_CREATE_NAMED_PIPE) {
				break; // 如果是创建命名管道，则直接跳出，不做额外处理
			}
			PECP_LIST ecpList = nullptr; // 初始化一个指向 ECP 列表的指针
			// 尝试从回调数据中获取 ECP 列表
			if (NT_SUCCESS(FltGetEcpListFromCallbackData(filterHandle, Data, &ecpList)) && ecpList != nullptr) { // 检查是否成功获取 ECP 列表且列表不为空

				// 检查 ECP 列表中是否包含特定 GUID 的 ECP
				if (Helper::checkEspListHasKernelGuid(filterHandle, ecpList, &GUID_ECP_PREFETCH_OPEN_FIX_VS_SHIT)) { // 检查是否存在预取相关的 ECP
					break; // 如果存在，跳出当前处理块
				}

				// 再次检查 ECP 列表中是否包含另一个特定 GUID 的 ECP
				if (Helper::checkEspListHasKernelGuid(
					filterHandle, ecpList, &GUID_ECP_CSV_DOWN_LEVEL_OPEN_FIX_WIN7)) { // 检查是否存在 CSV/降级打开相关的 ECP
					break; // 如果存在，跳出当前处理块
				}
			}

		} while (false); // 确保 do 块只执行一次，break 语句会直接跳到这里
		
		
		createStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;  //操作完成后还需要调用自定义的回调函数处理
		return createStatus; // 返回预操作回调状态
		
	};

	auto MinifilterCtxCleanUp(PFLT_CONTEXT Context, FLT_CONTEXT_TYPE ContextType)
		-> void {
		UNREFERENCED_PARAMETER(ContextType);
		freeStreamCtx(static_cast<_Minifilter_Stream_Context*>(Context));
	}


	//服务连接时通知回调函数
	auto ConnectNotifyCallback(
		IN PFLT_PORT ClientPort,
		IN PVOID ServerPortCookie,
		IN PVOID ConnectionContext,
		IN ULONG SizeOfContext,
		OUT PVOID* ConnectionPortCookie) -> NTSTATUS {
		ClientHandle = ClientPort;
		ExAcquireResourceExclusiveLite(&ClientLock, true); //上锁
		DebugPrint("a client was connect \n");
		ExReleaseResourceLite(&ClientLock);  //解锁
		return STATUS_SUCCESS;
	}

	//服务断开时通知回调函数
	auto DisconnectNotifyCallback(PVOID ConnectionCookie) -> void {
		DebugPrint("[Minifilter] DisconnectNotifyCallback: Client disconnected. ConnectionCookie: %p\n", ConnectionCookie); // 
		ExAcquireResourceExclusiveLite(&ClientLock, true);
		DebugPrint("[Minifilter] DisconnectNotifyCallback: ClientLock acquired.\n"); // 

		if (ClientHandle != NULL) {
			DebugPrint("[Minifilter] DisconnectNotifyCallback: Closing ClientHandle %p.\n", ClientHandle); // 
			FltCloseClientPort(filterHandle, &ClientHandle);
			ClientHandle = NULL; // 确保置为 NULL
			DebugPrint("[Minifilter] DisconnectNotifyCallback: ClientHandle closed and set to NULL.\n"); // 
		}
		else {
			DebugPrint("[Minifilter] DisconnectNotifyCallback: ClientHandle already NULL.\n"); // 
		}
		ExReleaseResourceLite(&ClientLock);
		DebugPrint("[Minifilter] DisconnectNotifyCallback: ClientLock released. Exiting.\n"); // 
	}

	


	auto UnloadMinifilter() {
		DebugPrint("[Minifilter] UnloadMinifilter: Entering.\n"); // 

		// 先关闭服务端端口
		if (CommPortHandle) {
			DebugPrint("[Minifilter] UnloadMinifilter: Closing CommPortHandle %p.\n", CommPortHandle); // 
			FltCloseCommunicationPort(CommPortHandle);
			CommPortHandle = NULL;
			DebugPrint("[Minifilter] UnloadMinifilter: CommPortHandle closed.\n"); // 
		}
		else {
			DebugPrint("[Minifilter] UnloadMinifilter: CommPortHandle is NULL, nothing to close.\n"); // 
		}

		// 客户端句柄的清理 (不在这里调用 FltCloseClientPort，由 DisconnectNotifyCallback 处理)
		if (ClientHandle != NULL) {
			DebugPrint("[Minifilter] UnloadMinifilter: ClientHandle %p is not NULL. Setting to NULL.\n", ClientHandle); // 
			ClientHandle = NULL; // 只是清除句柄变量，避免悬挂指针
		}
		else {
			DebugPrint("[Minifilter] UnloadMinifilter: ClientHandle already NULL.\n"); // 
		}

		// 注销过滤器
		if (filterHandle) {
			DebugPrint("[Minifilter] UnloadMinifilter: Unregistering filterHandle %p.\n", filterHandle); // 
			FltUnregisterFilter(filterHandle);
			filterHandle = NULL;
			DebugPrint("[Minifilter] UnloadMinifilter: Filter unregistered.\n"); // 
		}
		else {
			DebugPrint("[Minifilter] UnloadMinifilter: filterHandle is NULL, nothing to unregister.\n"); // 
		}

		// 最后删除 ClientLock
		if (LockInited) {
			DebugPrint("[Minifilter] UnloadMinifilter: Attempting to delete ClientLock.\n"); // 
			// 在删除前，获取并立即释放锁，确保没有其他线程持有
			ExAcquireResourceExclusiveLite(&ClientLock, true);
			ExReleaseResourceLite(&ClientLock);
			ExDeleteResourceLite(&ClientLock);
			LockInited = false; // 重置标志
			DebugPrint("[Minifilter] UnloadMinifilter: ClientLock deleted.\n"); // 
		}
		else {
			DebugPrint("[Minifilter] UnloadMinifilter: ClientLock not initialized, nothing to delete.\n"); // 
		}
		DebugPrint("[Minifilter] UnloadMinifilter: Exiting.\n"); // 
	}
	


	auto MiniFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags) -> NTSTATUS {
		DebugPrint("[Minifilter] MiniFilterUnload: Entering. Flags: 0x%X\n", Flags); // 
		KernelMsg::Uninstall(); // 先清理 KernelMsg 模块
		DebugPrint("[Minifilter] MiniFilterUnload: KernelMsg::Uninstall completed.\n"); // 
		UnloadMinifilter();     // 再清理 Minifilter 模块自身的资源
		DebugPrint("[Minifilter] MiniFilterUnload: UnloadMinifilter completed.\n"); // 
		return STATUS_SUCCESS;
	}
	
	auto Init(PDRIVER_OBJECT Driver) -> bool {
		PSECURITY_DESCRIPTOR securityDescriptor = NULL;
		OBJECT_ATTRIBUTES objectAttr = {};
		UNICODE_STRING portNameString;
		RtlInitUnicodeString(&portNameString, Ring3CommPort);

		UNICODE_STRING RegistryPathUnicode{};
		RtlInitUnicodeString(&RegistryPathUnicode, RegistryPath);
		InitializeMiniFilterRegedit(&RegistryPathUnicode, Altitude);

		auto ntStatus =FltRegisterFilter(Driver, &filterRegistration, &filterHandle);
		do {
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			}
			ntStatus = ExInitializeResourceLite(&ClientLock);
			if (NT_SUCCESS(ntStatus) == false) {
				return false;
			}
			LockInited = true;
			ntStatus = FltBuildDefaultSecurityDescriptor(&securityDescriptor,
				FLT_PORT_ALL_ACCESS);
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			}
			InitializeObjectAttributes(&objectAttr, &portNameString,
				OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
				NULL, securityDescriptor);

			ntStatus = FltCreateCommunicationPort(
				filterHandle, &CommPortHandle, &objectAttr, NULL,
				ConnectNotifyCallback, DisconnectNotifyCallback, NULL, 2);
			if (NT_SUCCESS(ntStatus) == false) {
				break;
			}
			ntStatus = FltStartFiltering(filterHandle);


		} while (false);
		if (NT_SUCCESS(ntStatus) == false) {
			UnloadMinifilter();
		}
		if (securityDescriptor) {
			FltFreeSecurityDescriptor(securityDescriptor);
			securityDescriptor = nullptr;
		}

		DebugPrint("[%s] status 0x%x", __FUNCTION__, ntStatus);
		return NT_SUCCESS(ntStatus);
	}

	auto InitializeMiniFilterRegedit(PUNICODE_STRING RegistryPath, PCWSTR Altitude)
		-> NTSTATUS {
		NTSTATUS Status = STATUS_SUCCESS;
		WCHAR KeyPath[255] = { 0 };
		HANDLE KeyHandle = (HANDLE)-1;
		//这个名字,是你的文件名字,因为我们的"驱动加载工具绿色版"是以驱动的文件名字写的服务
		WCHAR Instance[] = L"DuckDemo Instance";
		ULONG Flags = 0;

		RtlCopyMemory(KeyPath, RegistryPath->Buffer, RegistryPath->Length);
		wcscat_s(KeyPath, 255, L"\\Instances");
		Status = KdRegistry::CreateValueKey(KeyPath, (WCHAR*)L"DefaultInstance", REG_SZ,
			Instance, sizeof(Instance));

		if (FALSE != NT_SUCCESS(Status)) {
			if (FALSE != NT_SUCCESS(Status)) {
				//这个名字,是你的文件名字,因为我们的"驱动加载工具绿色版"是以驱动的文件名字写的服务
				wcscat_s(KeyPath, 255, L"\\DuckDemo Instance");

				Status = KdRegistry::CreateValueKey(KeyPath, (WCHAR*)L"Altitude", REG_SZ,
					(PVOID)Altitude,
					(ULONG)wcslen(Altitude) * 2);

				if (FALSE != NT_SUCCESS(Status)) {
					Status = KdRegistry::SetValueKey(KeyPath, (WCHAR*)L"Flags", REG_DWORD,
						&Flags, 4);
				}
			}
		}
		return Status;
	}



};