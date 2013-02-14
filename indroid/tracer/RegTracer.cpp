#include "indroid/tracer/RegTracer.h"

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
	
	void RegTracer::record_reg_read	( u2 index, u4 value )
	{
		
	}

	void RegTracer::record_reg_write	( u2 index, u4 value )
	{
		
	}

}; // end of namespace gossip_loccs

