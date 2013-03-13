#include "indroid/tracer/OpcodeTracer.h"

#include <cstdio>
#include <ctime>
#include <fstream>

//#include <sys/stat.h>
//#include <sys/types.h>
//#include <signal.h>

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
using std::vector;
using std::set;
using std::ifstream;

extern gossip_loccs::Filter filter;
namespace gossip_loccs
{
	struct InstRecord
	{
		u4 threadId;
		u4 pc;
	};

	OpcodeTracer::~OpcodeTracer()
	{
		GOSSIP( "total opcodes record num: %d\n", instUid_ );
		// destruct only when process ends.
		if ( traceFile_ != NULL )
			fclose ( traceFile_ );
		GOSSIP ( "CLOSE %s trace\n", traceFileName_.c_str() );
	}

	bool OpcodeTracer::init( const string & apkDir ) 
	{
		instUid_ = 0;
		apkDir_ = apkDir;

		char f[MaxLineLen] = {0};
		char g[MaxLineLen] = {0};

		// generates opcodes.bin full name
		snprintf ( f, MaxLineLen, "%s/opcode_%d.bin", apkDir_.c_str(), getpid() );
		snprintf ( g, MaxLineLen, "%s/opcodeSet_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		tracePoolFileName_ = string(g);
		return init_traceFile();
	}

	bool OpcodeTracer::init_traceFile() 
	{
		tracePoolFile_ = fopen ( tracePoolFileName_.c_str(), "wb" );

		if ( tracePoolFile_ == NULL )
		{
			GOSSIP( "open %s error!\n", tracePoolFileName_.c_str() );
			return false;
		}
		return Tracer::init_traceFile();
	}
	
	void OpcodeTracer::flush_traceFile() 
	{
		fflush( tracePoolFile_ );
		Tracer::flush_traceFile();
	}

	void OpcodeTracer::record_opcode( const u2 * const pc, u4 threadId, const Method * const method )
	{
		++instUid_;
		if ( !filter.record_should_be_opened(OpcodeFlag) )
			return ;

		static const Method * oldMethod = NULL;
		static u4 oldThreadId = 0xFFFFFFFF;
		static const u4 ThreadIdHead = 0xFFFFFFFF;
		static Insts ins; 

		// record threadId when it is changed
	    if ( oldThreadId != threadId )
		{
			if ( fwrite( &ThreadIdHead, sizeof(u4), 1, traceFile_ ) != 1 )
				GOSSIP( "write %s error when recording threadId head\n", traceFileName_.c_str() );
			if ( fwrite( &threadId, sizeof(u4), 1, traceFile_ ) != 1 )
				GOSSIP( "write %s error when recording threadId\n", traceFileName_.c_str() );

			oldThreadId = threadId;
		}

		/* record opcode contents */ 
	    u4 ipc = (u4) (pc);

		// record pc only
		if ( fwrite( &ipc, sizeof(u4), 1, traceFile_ ) != 1 )
		{
			GOSSIP( "write %s error\n", traceFileName_.c_str() );
		}
		
		if ( opcodeSet_.find( ipc ) == opcodeSet_.end() )
		{
			opcodeSet_.insert( ipc );

			/* must use assignment operation!!! */
			for ( size_t i = 0; i < InstNum; ++i )
				ins.in[i] = pc[i];

			if ( fwrite( &ipc, sizeof(u4), 1, tracePoolFile_ ) != 1 )
				GOSSIP( "write pool %s error when writing ipc\n", tracePoolFileName_.c_str() );
			if ( fwrite( &ins, sizeof(Insts), 1, tracePoolFile_ ) != 1 )
				GOSSIP( "write pool %s error when writing instRecord\n", tracePoolFileName_.c_str() );
		} 

		if ( oldMethod != method )
		{
			oldMethod = method;
			GOSSIP ( "Method changes to %s, %s\n", method->clazz->descriptor, method->name );
		}
	}

	u4 OpcodeTracer::get_instUid()
	{
		return instUid_;
	}

}; // end of namespace gossip_loccs

