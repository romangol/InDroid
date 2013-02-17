#include "indroid/Probe.h"
#include "indroid/tracer/OpcodeTracer.h"
#include "indroid/tracer/RegTracer.h"
#include "indroid/filter/Filter.h"
#include "indroid/utils/Utilproc.h"

using namespace std;
using namespace gossip_loccs;

// init a uidmap via util class firstly
UtilProc util;

// init a filter for class name and method name filtering
Filter filter;

// init tracers to record runtime data.
OpcodeTracer opcodeTracer;
RegTracer regTracer;

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

static bool traceFlag = false;

void diaos_monitor_mov( const u2 * const pc, const u4 * const fp, const Thread * const self )
{
	/*
    const Method* method = self->interpSave.method;

	if ( util.apk_should_be_traced() )
	{
		GOSSIP( "%08x, %s\n", pc[0], method->clazz->descriptor ); 
	}
	*/
}

bool diaos_start( const Method * const method )
{
	static bool initFlag = false;
	if ( initFlag ) return true;

	if ( util.apk_should_be_traced() ) // check the existence of class.dlist file
	{
		initFlag = diaos_init();
		/*
		if ( initFlag == true )
			GOSSIP( "InDroid starts for %s\n", method->clazz->descriptor ); 
		else
			GOSSIP( "InDroid fails to start for %s\n", method->clazz->descriptor ); 
		*/
	}

	return initFlag;
}

bool diaos_init()
{
	bool status = true;

	filter.init( util.get_apk_dir() );
	status &= opcodeTracer.init( util.get_apk_dir() );
	status &= regTracer.init( util.get_apk_dir() );

	return status;
}

void diaos_monitor_opcode ( const u2 * const pc, const u4 * const fp, const Thread * const self )
{
	if ( filter.class_should_be_traced( self->interpSave.method->clazz->descriptor ) )   
	{
		// set traceFlag once for a single opcode, so that in monitor_reg() the filter should not be invoked.
		traceFlag = true;
		opcodeTracer.record_opcode( pc, self->threadId, self->interpSave.method );
	}
	else
		traceFlag = false;
}

void diaos_monitor_reg( RegOpType type, const u4 * const fp, u2 index )
{
	if ( traceFlag ) 
		regTracer.record_reg( type, fp, index, opcodeTracer.get_instUid() );
}


#if 0
void diaos_monitor_func_call( const Method * const m )
{
	if ( traceFlag )   
		tracer.record_func_call( m );
}
void diaos_monitor_object( const Method * const m, const Object * const obj )
{
	if ( traceFlag )   
		tracer.extract_str(obj);
}
#endif
