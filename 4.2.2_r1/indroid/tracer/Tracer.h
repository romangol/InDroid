#ifndef _DIAOS_TRACER_H_ 
#define _DIAOS_TRACER_H_ 
#include <stdint.h>
#include <set>
#include <string>
#include <memory>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"
#include "indroid/Constant.h"

namespace gossip_loccs
{

class Tracer
{
public:
	virtual ~Tracer					();
	virtual bool init_traceFile		();
	virtual void flush_traceFile		();


protected:
	std::string		apkDir_;
	std::string		traceFileName_;
	FILE *			traceFile_;
	//std::shared_ptr<trFile> traceFile_;

	//low to high: obj in opcodes; param&retval of spec method; API; instructions; 
	//uint8_t			recordFlag_;
};


} // end of namespace loccs
#endif

