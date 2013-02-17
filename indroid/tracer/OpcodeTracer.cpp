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
		snprintf ( g, MaxLineLen, "%s/opcodePool_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		tracePoolFileName_ = string(g);
		return Tracer::init_traceFile();
	}

	void OpcodeTracer::record_opcode( const u2 * const pc, u4 threadId, const Method * const method )
	{
		++instUid_;

		/*
		if ( !(recordFlag_ & 0x08) )
		{
			this->instUid_++;
			return ;
		}
		*/

		static InstRecord r;

		/* record opcode contents */ 
	    r.threadId = threadId ;
	    r.pc = (u4) (pc);

		if ( opcodePool_.find( r.pc ) == opcodePool_.end() )
		{
			static Insts ins; 

			/* must use assignment operation!!! */
			for ( size_t i = 0; i < InstNum; ++i )
				ins.in[i] = pc[i];
			opcodePool_[r.pc] = ins;
		} 
	
		static time_t oldTime = 0;
		if ( 30 < time(NULL) - oldTime ) 
		{
			save_opcode_pool();
			time( &oldTime );
		}

		// write one Struct each time.
		if ( fwrite( &r, sizeof(InstRecord), 1, traceFile_ ) != 1 )
		{
			GOSSIP( "write %s error\n", traceFileName_.c_str() );
		}
		
	}

	void OpcodeTracer::save_opcode_pool()
	{
		static size_t poolSize = 0;
		if ( poolSize != opcodePool_.size() )
		{
			FILE * f = fopen ( tracePoolFileName_.c_str(), "wb" );
			if ( f == NULL )
			{
				GOSSIP( "open %s error!\n", tracePoolFileName_.c_str() );
				return;
			}

			for ( map<u4, Insts>::iterator it = opcodePool_.begin(); it != opcodePool_.end(); ++it )
			{
				if ( fwrite( &(it->first), sizeof(u4), 1, f ) != 1 )
					GOSSIP( "write pool %s error\n", tracePoolFileName_.c_str() );
				if ( fwrite( &(it->second), sizeof(Insts), 1, f ) != 1 )
					GOSSIP( "write pool %s error\n", tracePoolFileName_.c_str() );
			}

			fclose( f );
			poolSize = opcodePool_.size();
			GOSSIP( "New pool size %d\n", poolSize );
		}
	}

	u4 OpcodeTracer::get_instUid()
	{
		return instUid_;
	}

}; // end of namespace gossip_loccs

