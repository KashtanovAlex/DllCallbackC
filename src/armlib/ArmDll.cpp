/*
 *
 */

#include "Define.h"
#include "revision_data.h"
#include <cstring>
#include <iostream>
#include <functional>
//#include "CXCallbacksManager.h"

#define WH_SCRIPT_API WH_API_EXPORT

extern "C"
{
    typedef void (*PrintFN)();
    typedef void (*PrintIntFN)(int, int);

    std::function<void()> functionBest;
    std::function<void(int, int)> printInt;

    /// Exposed in script modules to return the script module revision hash.
    WH_SCRIPT_API char const* GetScriptRevisionHash()
    {
        return _HASH;
    }

    /// Exposed in script module to return the name of the script module
    /// contained in this shared library.
    WH_SCRIPT_API void RegisterFunctions(PrintFN printFn, PrintIntFN printIntFn)
    {
        functionBest = printFn;
        printInt = printIntFn;
    }

    /// Exposed in script modules to register all scripts to the ScriptMgr.
    WH_SCRIPT_API void Print()
    {
        if (!functionBest)
        {
            printf("!functionBest\n");
            return;
        }

        printf("Print\n");

        functionBest();
    }

    WH_SCRIPT_API void PrintValue(int value, int value1)
    {
        if (!printInt)
        {
            printf("!printInt\n");
            return;
        }

        printf("PrintValue %u/%u\n", value, value1);

        printInt(value, value1);
    }
} // extern "C"
