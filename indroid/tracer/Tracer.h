#ifndef _DIAOS_TRACER_H_
#define _DIAOS_TRACER_H_

#include <stdint.h>
#include <set>
#include <string>

#include "indroid/Monitor.h"
#include "indroid/Constant.h"

namespace gossip_loccs
{

class Tracer
{
public:
	~Tracer				();
	bool init_traceFile	();

protected:
	std::string		apkDir_;
	std::string		traceFileName_;
	FILE *			traceFile_;

	//low to high: obj in opcodes; param&retval of spec method; API; instructions; 
	uint8_t			recordFlag_;
};


} // end of namespace loccs
#endif

