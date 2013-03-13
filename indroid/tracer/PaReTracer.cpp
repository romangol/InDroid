#include "indroid/tracer/PaReTracer.h"

#include <cstdio>


// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
extern gossip_loccs::FuncTracer funcTracer;
extern gossip_loccs::Filter filter;

namespace gossip_loccs
{
	PaReTracer::~PaReTracer()
	{
	}

	bool PaReTracer::init() 
	{
		traceFile_ = funcTracer.get_traceFile();
		traceFileName_ = funcTracer.get_traceFileName();
		retjval = 0;
		memset(className, 0, sizeof(char)*ClassNameMaxLen );
		memset(methodName, 0, sizeof(char)*MethodNameMaxLen );
		memset(shortyName, 0, sizeof(char)*ShortyNameMaxLen );
		if (traceFile_ == NULL)
			return false;
		return true;
	}

	void PaReTracer::record_para( const Method * const m, u4* pr )
	{

		const char *s = m->shorty;
		int l = strlen( s );

		for ( int i = 0; i < l; i++ )
		{
			fprintf(traceFile_, "p[%d]: ", i);
			if ( s[i] == 'L' )
			{
				Object* o = (Object*) pr[i];
				if ( filter.object_should_be_traced(o) )
					record_obj(o);
			}
			else
			{
				GOSSIP("para file: %p", traceFile_);
				this->record_normal(s[i], pr + i, BASIC_TYPE);
				GOSSIP("para file: %p", traceFile_);
			}

				
			//fprintf(traceFile_, "\n" );
		}
		fflush(traceFile_);
	}

	void PaReTracer::record_retval()
	{
		/*
		if (! (recordFlag_ & 0x02))
			return ;
		*/
		//const char * sn = m->shorty;

		fprintf( traceFile_, "rv: " );
		if (shortyName[0] == 'L')
		{
			Object* o = (Object*) retjval;
			if ( filter.object_should_be_traced(o) )
				record_obj(o);
		}
		else
		{
			union myunion m;
			m.s = retjval;
			record_normal( shortyName[0], m.u, BASIC_TYPE );
		}
			
		//fprintf(traceFile_, "\n" );
		fflush(traceFile_);
	}

	void PaReTracer::record_temp_info ( const Method * const m, s8& rj )
	{
		retjval = rj;
		snprintf( className, ClassNameMaxLen, "%s", m->clazz->descriptor );
		snprintf( methodName, MethodNameMaxLen, "%s", m->name );
		snprintf( shortyName, ShortyNameMaxLen, "%s", m->shorty );

	}

	char* PaReTracer::get_className()
	{
		return className;
	}

	char* PaReTracer::get_methodName()
	{
		return methodName;
	}
}