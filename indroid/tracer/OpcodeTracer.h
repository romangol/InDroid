#ifndef _DIAOS_OPCODE_TRACER_H_
#define _DIAOS_OPCODE_TRACER_H_

#include <string>
#include <map>

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include "indroid/tracer/Tracer.h"

namespace gossip_loccs
{

struct Insts
{
	u2 in[InstNum];
};

class OpcodeTracer : public Tracer
{
public:
	~OpcodeTracer		();
	bool init			( const std::string & apkDir );
	void record_opcode	( const u2 * const pc, u4 threadId, const Method * const method );
	u4 get_instUid		();

private:
	void save_opcode_pool();
	bool init_traceFile	();
	std::string tracePoolFileName_;

	u4								instUid_;
	std::map<u4, std::string>		methodPool_;
	std::map<u4, Insts>	opcodePool_; 
};


} // end of namespace loccs

#endif
