#pragma once
#include "head.h"
#include <Windows.h>

namespace Tools {
    /**
     * @brief 简化创建线程的函数。
     * @param pFunctionAddress 线程函数的地址。
     * @param pParams 传递给线程函数的参数。
     * @return 返回新创建线程的句柄。
     */
    auto EasyCreateThread(void* pFunctionAddress, void* pParams)->HANDLE;
}