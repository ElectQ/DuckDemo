#pragma once
#include "head.h"


namespace KdRegistry {
	NTSTATUS NTAPI SetValueKey(WCHAR* RegKey, WCHAR* KeyName, ULONG ValueClass,
		void* Value, int vlen);
	NTSTATUS NTAPI CreateValueKey(WCHAR* RegKey, WCHAR* KeyName, ULONG ValueClass,
		void* Value, int vlen);
}