#ifndef _DIAOS_PROBE_H_
#define _DIAOS_PROBE_H_

//#include <stdint.h>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include "indroid/Constant.h"

using gossip_loccs::RegOpType;

extern "C" 
{
void diaos_monitor_mov( const u2 * const pc, const u4 * const fp, const Thread * const self );

bool diaos_start( const Method * const method );
bool diaos_init();

void diaos_monitor_opcode ( const u2 * const pc, const u4 * const fp, const Thread * const self, const Method * const method );
void diaos_monitor_object( const Method * const m, Object * obj );
void diaos_monitor_reg( RegOpType type, const u4 * const fp, u2 index );
void diaos_monitor_func_call( const Method * const method, const Method * const call );
void diaos_monitor_parameter(const Method * const m, u4* pr);
void diaos_monitor_temp_info(const Method * const m, s8& rj );
void diaos_monitor_retval(const Method * method);
void diaos_monitor_Objectl( const Method * const m, const Object * const obj);
}



#endif
