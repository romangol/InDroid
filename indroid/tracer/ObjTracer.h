#ifndef _DIAOS_OBJ_TRACER_H_
#define _DIAOS_OBJ_TRACER_H_

#include <string>
#include <map>

#include "indroid/tracer/Tracer.h"
#include "indroid/Constant.h"
#include "indroid/tracer/OpcodeTracer.h"
	


namespace gossip_loccs
{	

class ObjTracer : public Tracer
{
public:
	~ObjTracer			();
	bool init			( const std::string & apkDir );
	bool check_obj ( const Object * const obj );

	void record_obj		( Object * o);
	void record_normal	( char t, u4* v, ObjWriteMode flag = OBJECT_TYPE);
	void record_str		( const Object * const obj, const u2 * const str, size_t len, ObjWriteMode flag );

	void extract_str				(const Object * const obj, ObjWriteMode flag = OBJECT_TYPE );
	void extract_other			(const Object * const obj);
	void extract_location		(const Object * const obj);
	void extract_swapTestClass	(const Object * const obj);
	void extract_activRecord		( const Object * const obj );
	void extract_intent			( Object * obj );
	void extract_procRecord		( const Object* const obj );
	void extract_compName		( const Object * const obj );
	void extract_activInfo		( const Object* const obj );
	void extract_stringUri		( const Object* const obj );

	void modify_intent( Object * obj);
	void dump_obj( const Object * const obj );

private:
	//bool init_traceFile	();
};


} // end of namespace loccs

#endif
