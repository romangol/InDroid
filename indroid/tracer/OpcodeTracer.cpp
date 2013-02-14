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
using std::map;
using std::ifstream;
	
namespace gossip_loccs
{
	OpcodeTracer::~OpcodeTracer()
	{
		GOSSIP( "total opcodes record num: %d\n", instUid_ );
	}

	bool OpcodeTracer::init( const string & apkDir ) 
	{
		apkDir_ = apkDir;

		instUid_ = 0;
		char f[MaxLineLen] = {0};

		// generates opcodes.bin full name
		snprintf ( f, MaxLineLen, "%s/opcode_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		return Tracer::init_traceFile();
	}
	

	void OpcodeTracer::record_opcode( const u2 * const pc, u4 threadId, const Method * const method )
	{
		/*
		if ( !(recordFlag_ & 0x08) )
		{
			this->instUid_++;
			return ;
		}
		*/

		static InstRecord r;
		static time_t oldtime = 0;

		r.uid = instUid_++;

		/* record opcode contents */ 
	    r.threadId = threadId ;
	    r.pc = (u4) (pc);


		if ( opcodePool_.find( r.pc ) == opcodePool_.end() )
		{
			static std::vector<u2> op(InstNum); 

			/* must use assignment operation!!! */
			for ( size_t i = 0; i < InstNum; ++i )
				op[i] = pc[i];
			opcodePool_[r.pc] = op;
		} 
	

		// write one Struct each time.
		if ( fwrite( &r, sizeof(InstRecord), 1, traceFile_ ) != 1 )
		{
			GOSSIP( "write %s error\n", traceFileName_.c_str() );
		}
		
		if ( 30 < time(NULL) - oldtime ) 
		{
			fflush( traceFile_ );
			time( &oldtime );
		}
	}

	void OpcodeTracer::get_instUid()
	{
		return instUid_;
	}

}; // end of namespace gossip_loccs

