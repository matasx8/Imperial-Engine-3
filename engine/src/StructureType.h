#pragma once
#include <stdint.h>

namespace imp 
{
    typedef uint64_t StructureType;

    struct StructureHeader
    {
        void* pNext;
        StructureType structureType;
    };
}