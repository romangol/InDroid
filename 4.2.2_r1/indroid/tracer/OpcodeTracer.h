#ifndef _DIAOS_OPCODE_TRACER_H_
#define _DIAOS_OPCODE_TRACER_H_

#include <string>
#include <set>

#include "indroid/tracer/Tracer.h"
#include "indroid/filter/Filter.h"

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
	void flush_traceFile	();

private:
	void save_opcode_pool();
	bool init_traceFile	();
	std::string tracePoolFileName_;
	FILE * tracePoolFile_;

	u4								instUid_;
	std::set<u4>					opcodeSet_; 
};


} // end of namespace loccs

#endif
