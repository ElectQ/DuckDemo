#include "tools.h"


namespace Tools {

	auto FreeCopiedString(PUNICODE_STRING p1) -> void {
		if (p1) {
			if (p1->Buffer) {
				ExFreePoolWithTag(p1->Buffer, poolTag);
			}
			ExFreePoolWithTag(p1, poolTag);
		}
	}
	ULONG poolTag = 'Duck';

	auto CopyUnicodeStringWithAllocPagedMemory(PUNICODE_STRING* p1,PUNICODE_STRING p2) -> bool {

	const auto target 	=  static_cast<PUNICODE_STRING>( ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), poolTag));
	if (target == FALSE) {
		return FALSE;
	}
	bool success = TRUE;
	do
	{
		memset(target, 0x00, sizeof(UNICODE_STRING));
		target->Length = p2->Length;
		target->MaximumLength = p2->MaximumLength;
		target->Buffer = static_cast<PWCH>(ExAllocatePoolWithTag(PagedPool, target->MaximumLength, poolTag));
		if (target->Buffer == nullptr) {
			return FALSE;
		}
		memcpy(target->Buffer, p2->Buffer, p2->MaximumLength);
		success = true;
		*p1 = target;

	} while (FALSE);
	if (success == false && (target != nullptr)) {
		FreeCopiedString(target);
	}
	return success;
	}
}