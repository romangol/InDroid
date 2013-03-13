#ifndef _DIAOS_REG_TRACER_H_
#define _DIAOS_REG_TRACER_H_

#include <string>

#include "indroid/tracer/Tracer.h"
#include "indroid/Constant.h"

namespace gossip_loccs
{

class RegTracer : public Tracer
{
public:
	~RegTracer			();
	bool init			( const std::string & apkDir );
	void record_reg		( RegOpType type, const u4 * const fp, u2 index, u4 instUid );

private:

};


} // end of namespace loccs

#endif
