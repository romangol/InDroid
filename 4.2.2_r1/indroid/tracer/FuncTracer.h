#ifndef _DIAOS_FUNC_TRACER_H_
#define _DIAOS_FUNC_TRACER_H_

#include <string>

#include "indroid/tracer/Tracer.h"
#include "indroid/Constant.h"

namespace gossip_loccs
{

class FuncTracer : public Tracer
{
public:
	~FuncTracer			();
	bool init			( const std::string & apkDir );
	void record_func	( const Method * const m, u4 instUid );
	FILE* get_traceFile ();
	std::string get_traceFileName		();

private:
	//bool init_traceFile	();
};


} // end of namespace loccs

#endif
