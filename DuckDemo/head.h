#pragma once
#include <fltKernel.h>
#include <intrin.h>
#include <ntddk.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ndis.h>
#include <ntimage.h>
#include <bcrypt.h>
#include <ntdef.h>


#include "minifilter.h"
#define DebugPrint(...) \
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)


#include "callback.h"
#include "KdRegistry.h"
#include "helper.h"
#include "tools.h"
#include "kernelmsg.h"