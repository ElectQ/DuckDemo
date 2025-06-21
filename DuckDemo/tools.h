#pragma once
#include "head.h"

namespace Tools {
	extern ULONG poolTag;

	auto FreeCopiedString(PUNICODE_STRING p1)->void;
	auto CopyUnicodeStringWithAllocPagedMemory(PUNICODE_STRING* p1,PUNICODE_STRING p2) -> bool;
}