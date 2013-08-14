#include "indroid/tracer/Tracer.h"

#include <cstdio>
#include <fstream>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

namespace gossip_loccs
{
using std::string;
using std::set;
using std::ifstream;

	Tracer::~Tracer()
	{
		// destruct only when process ends.
		if ( traceFile_ != NULL )
			fclose ( traceFile_ );
		GOSSIP ( "CLOSE %s trace\n", traceFileName_.c_str() );
	}

	bool Tracer::init_traceFile() 
	{
		// init trace file for Tracer
		// should not use append mode, because a tracer is a single static class instance.
		traceFile_ = fopen ( traceFileName_.c_str(), "wb" );

		if ( traceFile_ == NULL )
		{
			GOSSIP( "open %s error!\n", traceFileName_.c_str() );
			// dvmAbort(); // harsh!!!
			return false;
		}
		return true;
	}
	
	void Tracer::flush_traceFile() 
	{
		fflush( traceFile_ );
	}
}; // end of namespace gossip_loccs

