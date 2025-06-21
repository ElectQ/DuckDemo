#include "KdRegistry.h"


namespace KdRegistry {
	NTSTATUS NTAPI SetValueKey(WCHAR* RegKey, WCHAR* KeyName, ULONG ValueClass,
		void* Value, int vlen) {
		PAGED_CODE();
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

		UNICODE_STRING UnicodeKey;
		RtlInitUnicodeString(&UnicodeKey, RegKey);

		OBJECT_ATTRIBUTES ObjectAttributes;
		InitializeObjectAttributes(&ObjectAttributes, &UnicodeKey,
			OBJ_CASE_INSENSITIVE, NULL, NULL);
		HANDLE hKey;
		ntStatus = ZwOpenKey(&hKey, GENERIC_ALL, &ObjectAttributes);
		if (!NT_SUCCESS(ntStatus)) {
			// DebugPrint(("打开注册表项失败！\n"));
			return ntStatus;
		}

		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, KeyName);
		ntStatus = ZwSetValueKey(hKey, &valueName, NULL, ValueClass, Value, vlen);

		if (!NT_SUCCESS(ntStatus)) {
			return FALSE;
		}
		ZwClose(hKey);
		return ntStatus;
	}
	NTSTATUS NTAPI CreateValueKey(WCHAR* RegKey, WCHAR* KeyName, ULONG ValueClass,
		void* Value, int vlen) {
		PAGED_CODE();
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

		// 创建或打开某注册表项目
		UNICODE_STRING RegUnicodeString;
		HANDLE hRegister;

		// 初始化UNICODE_STRING字符串
		RtlInitUnicodeString(&RegUnicodeString, RegKey);

		OBJECT_ATTRIBUTES objectAttributes;
		InitializeObjectAttributes(&objectAttributes, &RegUnicodeString,
			OBJ_CASE_INSENSITIVE, NULL, NULL);
		
		ULONG ulResult;
		// 创建或带开注册表项目
		ntStatus = ZwCreateKey(&hRegister, KEY_ALL_ACCESS, &objectAttributes, 0,
			NULL, REG_OPTION_NON_VOLATILE, &ulResult);
		if (NT_SUCCESS(ntStatus)) { //关闭注册表句柄
			ZwClose(hRegister);
			ntStatus = SetValueKey(RegKey, KeyName, ValueClass, Value, vlen);
		}
		return ntStatus;
	}
};