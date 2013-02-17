#ifndef _DIAOS_REG_TRACER_H_
#define _DIAOS_REG_TRACER_H_

#include <string>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include "indroid/tracer/Tracer.h"

namespace gossip_loccs
{

class RegTracer : public Tracer
{
public:
	~RegTracer				();
	bool init				( const std::string & apkDir );
	void record_reg_read	( u2 index, u4 value );
	void record_reg_write	( u2 index, u4 value );

private:
	bool init_traceFile	();
};


} // end of namespace loccs

#endif
