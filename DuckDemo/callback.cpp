
#include "callback.h"


namespace Callback {
	bool CreateProcessisInstalled = FALSE;

	// 【修改】监控名单 - 只有这些进程会发送到R3处理
	const PCWSTR MonitorProcesses[] = {
		L"cmd.exe",
		L"powershell.exe", 
		L"whoami.exe"
	};
	const ULONG MonitorProcessCount = sizeof(MonitorProcesses) / sizeof(MonitorProcesses[0]);
	// 【删除】删除剩余的白名单数组
	// 原来的WhitelistProcesses数组已经被MonitorProcesses替代

	// 【修改】检查进程是否在监控名单中（需要发送到R3处理）
	auto IsProcessInMonitorList(PCUNICODE_STRING processPath) -> bool {
		if (processPath == nullptr || processPath->Buffer == nullptr || processPath->Length == 0) {
			return false;
		}

		// 提取文件名（去除路径）
		PWCH fileName = processPath->Buffer;
		USHORT fileNameLength = processPath->Length;
		
		// 从后往前找最后一个反斜杠
		for (int i = processPath->Length / sizeof(WCHAR) - 1; i >= 0; i--) {
			if (processPath->Buffer[i] == L'\\') {
				fileName = &processPath->Buffer[i + 1];
				fileNameLength = processPath->Length - ((i + 1) * sizeof(WCHAR));
				break;
			}
		}

		// 检查是否在监控名单中
		for (ULONG i = 0; i < MonitorProcessCount; i++) {
			SIZE_T whitelistLen = wcslen(MonitorProcesses[i]);
			
			// 检查长度是否匹配
			if (fileNameLength / sizeof(WCHAR) == whitelistLen) {
				// 不区分大小写比较文件名
				if (_wcsnicmp(fileName, MonitorProcesses[i], whitelistLen) == 0) {
					return true;
				}
			}
		}
		
		return false;
	}

	auto CreateProcessNotifyRoutineEx(PEPROCESS Process, HANDLE ProcessId,PPS_CREATE_NOTIFY_INFO CreateInfo) -> void {
		bool isCreate = CreateInfo != nullptr;
		KIRQL currentIrql = KeGetCurrentIrql();
		DebugPrint("[Callback] CreateProcessNotifyRoutineEx called, PID: %d, isCreate: %s, Current IRQL = %d\n", ProcessId, isCreate ? "true" : "false", currentIrql);
		if (isCreate) {
			DebugPrint("process create, pid: %d file: %wZ \n", ProcessId,
				CreateInfo->ImageFileName);

			// 【修改】检查监控名单 - 只有在监控名单中的进程才发送消息
			if (IsProcessInMonitorList(CreateInfo->ImageFileName)) {
				DebugPrint("[Callback] Process %wZ is in MONITOR list, will send to R3 for decision\n", CreateInfo->ImageFileName);

				size_t msgSize = sizeof(_MsgCreateProcess);
				msgSize += (size_t)CreateInfo->ImageFileName->Length + 2;  //动态计算buffer
				msgSize += (size_t)CreateInfo->CommandLine->Length + 2;

				_MsgCreateProcess* msg = reinterpret_cast<_MsgCreateProcess*>(ExAllocatePoolWithTag(NonPagedPool, msgSize, Tools::poolTag));  //内核模式下分配堆内存
				DebugPrint("[Callback] Memory allocation with NonPagedPool, ptr: %p\n", msg);
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
				DebugPrint("[Callback] Sending create process event to R3, msgSize: %lu\n", (unsigned long)msgSize);
				bool blockResult = KernelMsg::SendCreateProcessEvent(msg, msgSize);  //当客户端返回需要阻止进程创建，则修改R0的Flag位
				
				DebugPrint("========================================\n");
				DebugPrint("=== [CALLBACK FINAL DECISION] ===\n");
				DebugPrint("[Callback] *** DECISION FROM DRIVER: %s ***\n", blockResult ? "BLOCK" : "ALLOW");
				DebugPrint("[Callback] *** PROCESS: %wZ ***\n", CreateInfo->ImageFileName);
				DebugPrint("[Callback] *** PID: %d ***\n", ProcessId);
				
				if (blockResult) {
					DebugPrint("=== [BLOCKING PROCESS CREATION] ===\n");
					DebugPrint("[Callback] *** SETTING STATUS_ACCESS_DENIED ***\n");
					DebugPrint("[Callback] *** PROCESS %d WILL BE BLOCKED ***\n", ProcessId);
					CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
					DebugPrint("=== [PROCESS BLOCKED SUCCESSFULLY] ===\n");
				} else {
					DebugPrint("=== [ALLOWING PROCESS CREATION] ===\n");
					DebugPrint("[Callback] *** PROCESS %d WILL BE ALLOWED ***\n", ProcessId);
					DebugPrint("=== [PROCESS ALLOWED SUCCESSFULLY] ===\n");
				}
				DebugPrint("========================================\n");
			} while (false);

			if (msg) {
				ExFreePoolWithTag(msg, Tools::poolTag);
				msg = nullptr;
			}
		} else {
			// 【新增】不在监控名单中的进程，直接允许（不发送到R3）
			DebugPrint("[Callback] Process %wZ NOT in monitor list, allowing directly\n", CreateInfo->ImageFileName);
			// 直接返回，不拦截
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