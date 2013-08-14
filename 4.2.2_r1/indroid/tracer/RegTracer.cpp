#include "indroid/tracer/RegTracer.h"

#include <cstdio>
#include <fstream>
#include <map>


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
	static const u2 READ_TYPE = 0x10;
	static const u2 WRITE_TYPE = 0x09;
	
	RegTracer::~RegTracer()
	{
	}

	bool RegTracer::init( const string & apkDir ) 
	{
		apkDir_ = apkDir;
		char f[MaxLineLen] = {0};

		// generates opcodes.bin full name
		snprintf ( f, MaxLineLen, "%s/reg_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		return Tracer::init_traceFile();
	}
	
	void RegTracer::record_reg ( RegOpType type, const u4 * const fp, u2 index, u4 instUid )
	{
		if ( fwrite( &type, sizeof(type), 1, traceFile_ ) != 1 )
			GOSSIP( "RegTracer record type failed\n");

		if ( fwrite( &instUid, sizeof(u4), 1, traceFile_ ) != 1 )
			GOSSIP( "RegTracer record instUid failed\n");

		if ( fwrite( &index, sizeof(u2), 1, traceFile_ ) != 1 )
			GOSSIP( "RegTracer record index failed\n");

		if( type == REG_READ_FLOAT || type == REG_READ_WIDE || type == REG_READ_DOUBLE || 
			type == REG_WRITE_FLOAT || type == REG_WRITE_WIDE || type == REG_WRITE_DOUBLE )
		{
			if ( fwrite( fp, sizeof(u4), 2, traceFile_ ) != 2 )
				GOSSIP( "RegTracer record long value failed\n");
		}
		else
		{
			if ( fwrite( fp, sizeof(u4), 1, traceFile_ ) != 1 )
				GOSSIP( "RegTracer record value failed\n");
		}
	}

}; // end of namespace gossip_loccs

