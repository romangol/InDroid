#ifndef _DIAOS_TRACER_H_
#define _DIAOS_TRACER_H_

#include <stdint.h>
#include <set>
#include <string>


#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include "indroid/monitor.h"
#include "indroid/constant.h"

namespace gossip_loccs
{

enum ObjWriteMode { ERROR, TO_FUNC, TO_OPC, OPC_OBJ, OPC_STR, FUNC_OBJ, FUNC_STR };
enum ObjType { TEST_OBJ, INTEGER, DOUBLE, STR_OBJ, 
	INTENT_OBJ, CMPNM_OBJ, ACTINFO_OBJ, PROREC_OBJ, ACTREC_OBJ, LOCAT_OBJ };
/*
const static uint8_t TO_FUNC 				= 1;
const static uint8_t TO_OPC					= 2;
const static uint8_t OPC_OBJ     			= 3;
const static uint8_t OPC_STR				= 4;
const static uint8_t FUNC_STR				= 5;
const static uint8_t FUNC_OBJ				= 6;
*/

//fuck gcc!!
union myunion {
	double d;
	u4 u[2];
	s8 s;
}; 

class Tracer : public Monitor
{
public:
	void record_func_call		( const Method * const m );
	// void record_opcode			( const u2 * const pc, const u4 * const fp, const Thread * const self, const Method * const method );
	void record_opcode			( u4 threadId, const Method* curMethod, const u2* pc, const u4 (&regTable)[RegMaxNum] );
	// void record_obj				( const Object * const obj );
	void record_obj 			( Object * obj, ObjWriteMode flag );
	// void extract_str			( const Object * const obj );

	void record_para_retval		( Object * obj);
	void record_normal			( char t, u4 *v, ObjWriteMode flag );

	uint8_t get_recordFlag		() { return recordFlag_; }

	bool init_recordFlag		();
	void change_flag			( ObjWriteMode &flag, bool b = true );


	void monitor_obj			( Object * obj );	
	void monitor_para			( const Method * const m, u4* pr);
	void monitor_retval			( const char* sn, s8& rj);
	void retvalid               ();

	void extract_intent         ( Object * obj, ObjWriteMode flag );
	void extract_compName		( const Object * const obj, ObjWriteMode flag );
	void extract_str			( Object * obj, ObjWriteMode flag );
	void extract_activInfo		( const Object * const obj, ObjWriteMode flag );
	void extract_procRecord		( const Object * const obj, ObjWriteMode flag );
	void extract_activRecord	( const Object * const obj, ObjWriteMode flag );
	void extract_location		( const Object * const obj, ObjWriteMode flag );
	//just for test!!!!
	void extract_swapTestClass  ( const Object * const obj, ObjWriteMode flag );

	void modify_intent			( Object * obj );

	void dump_obj				( const Object * const obj );
	bool check_obj				( const Object * const obj );

private:
	FILE *			fpFuncs_;
	FILE * 			fpObj_;
	FILE * 			fpOpcode_;
	//FILE *		fpStr_;

	uint32_t    	instId_;
	uint32_t    	instUid_;
	std::string		apkDir_;

	// non copyable
	Tracer (Tracer const&);
	Tracer& operator= (Tracer const&);

	// void record_str				( const u2 * const str, size_t len );
	void record_str( const u2 * const str, size_t len, ObjWriteMode mode );
	void reg_table_to_Buff( const u4 (&regTable)[RegMaxNum], Buff& buff );


	//low to high: obj in opcodes; param&retval of spec method; API; instructions; 
	uint8_t			recordFlag_;
};


} // end of namespace loccs

#endif
