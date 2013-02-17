#include "indroid/Probe.h"
#include "indroid/tracer/OpcodeTracer.h"
#include "indroid/filter/Filter.h"
#include "indroid/utils/Utilproc.h"

using namespace std;
using namespace gossip_loccs;

// init a uidmap via util class firstly
static UtilProc util;

// init a filter for class name and method name filtering
static Filter filter;

// init a tracer to record runtime data.
static OpcodeTracer opcodeTracer;

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

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

	return status;
}

void diaos_monitor_opcode ( const u2 * const pc, const u4 * const fp, const Thread * const self )
{
	if ( filter.class_should_be_traced( self->interpSave.method->clazz->descriptor ) )   
		opcodeTracer.record_opcode( pc, self->threadId, self->interpSave.method );
}

#if 0
void diaos_monitor_object( const Method * const m, const Object * const obj )
{
	if ( filter.class_should_be_traced( m->clazz->descriptor ) )   
		tracer.extract_str(obj);
}

void diaos_monitor_reg_read( const Method * const m, u2 index, u4 value )
{
	if ( filter.class_should_be_traced( m->clazz->descriptor ) )   
		tracer.record_reg_read( index, value );
}

void diaos_monitor_reg_write( const Method * const m, u2 index, u4 value )
{
	if ( filter.class_should_be_traced( m->clazz->descriptor ) )   
		tracer.record_reg_write( index, value );
}

void diaos_monitor_func_call( const Method * const m )
{
	if ( filter.class_should_be_traced ( m->clazz->descriptor ) )
		tracer.record_func_call( m );
}
#endif
