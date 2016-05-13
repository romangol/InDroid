#ifndef _DIAOS_TOOLS_H_
#define _DIAOS_TOOLS_H_
#include <stdint.h>
#include "Dalvik.h"

extern "C" 
{	
	void dexdumpOut(const Method * const m , FILE * file);		
}

#endif

