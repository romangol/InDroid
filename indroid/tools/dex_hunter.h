#ifndef _DEX_HUNTER_H_
#define _DEX_HUNTER_H_
#include <stdint.h>
#include "Dalvik.h"

extern "C"
{
        void dexdumpTrueOut(const Method * const m , FILE * file, char const * dumppath);
}

#endif
