#pragma once
#include <cstdio>

namespace imp
{
	typedef int (*LogFunc)(const char*, ...);
	inline LogFunc g_Log = printf;
}