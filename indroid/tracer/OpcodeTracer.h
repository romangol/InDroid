#ifndef _DIAOS_OPCODE_TRACER_H_
#define _DIAOS_OPCODE_TRACER_H_

#include <string>
#include <vector>
#include <map>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include "indroid/tracer/Tracer.h"

namespace gossip_loccs
{

class OpcodeTracer : public Tracer
{
public:
	~OpcodeTracer		();
	bool init			( const std::string & apkDir );
	void record_opcode	( const u2 * const pc, u4 threadId, const Method * const method );
	u4 get_instUid		();

private:
	bool init_traceFile	();

	u4								instUid_;
	std::map<u4, std::string>		methodPool_;
	std::map<u4, std::vector<u2> >	opcodePool_; 
};


} // end of namespace loccs

#endif
