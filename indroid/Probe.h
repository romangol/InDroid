#ifndef _DIAOS_PROBE_H_
#define _DIAOS_PROBE_H_

#include <stdint.h>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"


extern "C" {
void diaos_monitor_mov( const u2 * const pc, const u4 * const fp, const Thread * const self );

bool diaos_start( const Method * const method );
bool diaos_init();
void diaos_monitor_opcode ( const u2 * const pc, const u4 * const fp, const Thread * const self );
void diaos_monitor_object( const Method * const m, const Object * const obj );
void diaos_monitor_reg_read( const Method * const m, u2 index, u4 value );
void diaos_monitor_reg_write( const Method * const m, u2 index, u4 value );
void diaos_monitor_func_call( const Method * const m );
}



#endif
