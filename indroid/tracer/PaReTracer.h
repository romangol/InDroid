#ifndef _DIAOS_PARE_TRACER_H_
#define _DIAOS_PARE_TRACER_H_

#include <string>
#include <map>

#include "indroid/tracer/ObjTracer.h"
#include "indroid/tracer/FuncTracer.h"
#include "indroid/filter/Filter.h"
	


namespace gossip_loccs
{	
	class PaReTracer : public ObjTracer
	{
	public:
		~PaReTracer			();
		bool init			();
		void record_retval	();
		void record_para		( const Method * const m, u4* pr );
		void record_temp_info ( const Method * const m, s8& rj );
		char* get_className();
		char* get_methodName();

	private:
		s8 				retjval;
		char className[ClassNameMaxLen];
		char methodName[MethodNameMaxLen];
		char shortyName[ShortyNameMaxLen];
	};

}

#endif