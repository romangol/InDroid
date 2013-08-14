#include "indroid/tracer/FuncTracer.h"

#include <cstdio>


// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
	
namespace gossip_loccs
{
	FuncTracer::~FuncTracer()
	{
	}

	bool FuncTracer::init( const string & apkDir ) 
	{
		apkDir_ = apkDir;
		char f[MaxLineLen] = {0};

		// generates opcodes.bin full name
		snprintf ( f, MaxLineLen, "%s/func_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		return Tracer::init_traceFile();
	}
	
	void FuncTracer::record_func ( const Method * const m, u4 instUid )
	{
		/*
		if ( !(recordFlag & 0x04) )
			return ;
		*/

		//instruction id
		//GOSSIP("func file: %p", traceFile_);
		fprintf( traceFile_, "instUid %u||", instUid );
		fprintf ( traceFile_, "%s||%s||%s\n", m->clazz->descriptor, m->name, m->shorty ); 

		//fflush ( traceFile_);
	    
		// GOSSIP( "funcs: %s,%s,%s\n", m->clazz->descriptor,m->name, m->shorty);
	}

	FILE* FuncTracer::get_traceFile()
	{
		return traceFile_;
	}

	string FuncTracer::get_traceFileName()
	{
		return traceFileName_;
	}
#if 0

#endif
}; // end of namespace gossip_loccs



