
#include "callback.h"


namespace Callback {
	bool CreateProcessisInstalled = FALSE;
	auto CreateProcessNotifyRoutineEx(PEPROCESS Process, HANDLE ProcessId,PPS_CREATE_NOTIFY_INFO CreateInfo) -> void {
		bool isCreate = CreateInfo != nullptr;
		if (isCreate) {
			DebugPrint("process create, pid: %d file: %wZ \n", ProcessId,
				CreateInfo->ImageFileName);

			size_t msgSize = sizeof(_MsgCreateProcess);
			msgSize += (size_t)CreateInfo->ImageFileName->Length + 2;  //动态计算buffer
			msgSize += (size_t)CreateInfo->CommandLine->Length + 2;

			_MsgCreateProcess* msg = reinterpret_cast<_MsgCreateProcess*>(ExAllocatePoolWithTag(PagedPool, msgSize, Tools::poolTag));
			do {
				if (msg == nullptr) {
					break;
				}
				memset(msg, 0x0, msgSize);
				msg->packetHeader.magicNum = 0x1337;
				msg->packetHeader.type = _ClientMsgType::kCreateProcess;
				msg->pid = reinterpret_cast<uint32_t>(ProcessId);
				msg->ppid = reinterpret_cast<uint32_t>(CreateInfo->ParentProcessId);
				msg->pathSize = CreateInfo->ImageFileName->Length + sizeof(wchar_t); // +2是因为wchar是00结尾
				msg->commandSize = CreateInfo->CommandLine->Length + sizeof(wchar_t);

				memcpy(msg->path, CreateInfo->ImageFileName->Buffer, msg->pathSize);


				msg->commandOffset += sizeof(_MsgClientPacket) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(size_t) + sizeof(size_t) + sizeof(size_t) + msg->pathSize;
				memcpy((void*)((uint64_t)msg + msg->commandOffset),
					CreateInfo->CommandLine->Buffer, msg->commandSize);
				if (KernelMsg::SendCreateProcessEvent(msg, msgSize)) {  //当客户端返回需要阻止进程创建，则修改R0的Flag位
					CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
				}
			} while (false);

			if (msg) {
				ExFreePoolWithTag(msg, Tools::poolTag);
				msg = nullptr;
			}

		}
		else {
			DebugPrint("process eixt, pid: %d \n", ProcessId);
		}
	}

	auto Uninstall()->void {
		DebugPrint("[Callback] Uninstall: Entering.\n"); // 
		if (CreateProcessisInstalled == TRUE) {
			DebugPrint("[Callback] Uninstall: Attempting to unregister CreateProcessNotifyRoutineEx.\n"); // 
			NTSTATUS status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineEx, TRUE);
			if (NT_SUCCESS(status)) {
				CreateProcessisInstalled = FALSE; // 成功注销后重置标志
				DebugPrint("[Callback] Uninstall: CreateProcessNotifyRoutineEx unregistered successfully.\n"); // 
			}
			else {
				DebugPrint("[Callback] Uninstall: Failed to unregister CreateProcessNotifyRoutineEx! Status: 0x%X\n", status); // 
				// 如果注销失败，这可能意味着你的回调仍然被调用，是潜在的崩溃点
			}
		}
		else {
			DebugPrint("[Callback] Uninstall: CreateProcessNotifyRoutineEx not installed or already uninstalled.\n"); // 
		}
		DebugPrint("[Callback] Uninstall: Exiting.\n"); // 
	}

	auto Init(PDRIVER_OBJECT driver) ->bool {
		auto Ntsatus = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineEx, FALSE);  //注册内核回调
		do {
			if (NT_SUCCESS(Ntsatus) == FALSE) {
				NT_ASSERT(FALSE);
				break;
			}
			CreateProcessisInstalled = TRUE;
		} while (false);
		DebugPrint("%s status: %08X \n", __FUNCTIONW__, Ntsatus);
		return NT_SUCCESS(Ntsatus);
	}
}