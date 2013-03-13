#include "indroid/tracer/ObjTracer.h"

#include <cstdio>

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
extern gossip_loccs::OpcodeTracer opcodeTracer;

namespace gossip_loccs
{
	const static char * ObjString = "Ljava/lang/String;";
	const static char * ObjIntent = "Landroid/content/Intent;";
	const static char * ObjLocat = "Landroid/location/Location;";
	const static char * ObjCmpNm = "Landroid/content/ComponentName;";
	const static char * ObjActInfo = "Landroid/content/pm/ActivityInfo;";
	const static char * ObjProRec = "Lcom/android/server/am/ProcessRecord;";
	const static char * ObjActRec = "Lcom/android/server/am/ActivityRecord;";
	const static char * ObjTest = "Lcom/swapTest/SwapTestActivity$TestClass;";

	ObjTracer::~ObjTracer()
	{

	}

	bool ObjTracer::init( const string & apkDir ) 
	{
		apkDir_ = apkDir;
		char f[MaxLineLen] = {0};

		// generates opcodes.bin full name
		snprintf ( f, MaxLineLen, "%s/obj_%d.bin", apkDir_.c_str(), getpid() );
		traceFileName_ = string(f);
		return Tracer::init_traceFile();
	}

	bool ObjTracer::check_obj( const Object * const obj )
	{
		if ( obj == NULL )
			return false;
		if ( (u4)(obj->clazz) <= 65536 || obj->clazz == NULL )
			return false;
		if ( obj->clazz->descriptor == NULL )
			return false;
		return true;
	}

	void ObjTracer::record_normal( char t, u4* v, ObjWriteMode flag)
	{
		const unsigned char n = '\n';
		
		if ( flag == BASIC_TYPE )
		{
			//GOSSIP("normal  :::::::%p", traceFile_);
			//GOSSIP("normal  filename %s", traceFileName_.c_str());
			fprintf( traceFile_, "instUid %u", opcodeTracer.get_instUid());

			if ( t == 'I' || t == 'Z' || t == 'S')
				fprintf(traceFile_, " #%d: ", INTEGER);

			if ( t == 'D' )
				fprintf(traceFile_, " #%d: ", DOUBLE);
		}

		if ( t == 'I' || t == 'Z' || t == 'S')
			fprintf(traceFile_, " %d ", *v);

		if ( t == 'D' )
		{
			union myunion m;
			m.u[0] = *v;
			m.u[1] = *(v+1);
			fprintf(traceFile_, " %lf ", m.d );
		}	

		fwrite( &n, 1, 1, traceFile_ );
		fflush( traceFile_ );

	}

	void ObjTracer::record_str( const Object * const obj, const u2 * const str, size_t len, ObjWriteMode flag )
	{
		const unsigned char n = '\n' ;
		const unsigned char space = ' ';

		if ( flag == BASIC_TYPE )
		{
			fprintf( traceFile_, "instUid %u #%d: %p\n", opcodeTracer.get_instUid(), STR_OBJ, obj);
		}

		if ( len >= StrMaxLen )
			len = StrMaxLen;
		fwrite( &space, 1, 1, traceFile_ );
		//fwrite( &len, sizeof(size_t), 1, f );
		fwrite( str, 1, len, traceFile_ );
		//if ( flag == OPC_STR || FUNC_STR )
		fwrite( &n, 1, 1, traceFile_ );
		fflush( traceFile_ );
	}


	void ObjTracer::record_obj( Object *obj )
	{
		u4 hash = BKDRHash (obj->clazz->descriptor);

		if ( hash == BKDRHash(ObjString) )
			this->extract_str( obj, BASIC_TYPE );
		
		else if ( hash == BKDRHash(ObjIntent) )
		{
			this->extract_intent( obj );
			//try to modify intent
			/*if ( 0 == strcmp(apkDir_.c_str(),"/data/data/com.example.testsms")  )
				this->modify_intent( obj );*/
		}

		else if ( hash ==  BKDRHash(ObjLocat) )
			this->extract_location( obj );

		else if ( hash ==  BKDRHash(ObjCmpNm) )
			this->extract_compName( obj );

		else if ( hash ==  BKDRHash(ObjActInfo) )
			this->extract_activInfo( obj );

		else if ( hash ==  BKDRHash(ObjProRec) )
			this->extract_procRecord( obj );

		else if ( hash ==  BKDRHash(ObjActRec) )
			this->extract_activRecord( obj );

		else if ( hash == BKDRHash(ObjTest) )
			this->extract_swapTestClass( obj );

		else
		{
			//in object filter, but not provided by diaos
			this->extract_other(obj);
		}
				
		
	}

	void ObjTracer::extract_str(const Object * const obj, ObjWriteMode flag)
	{
		if ( !check_obj ( obj ) )
		{	
			ALOG(LOG_VERBOSE, "YWB", "OBJECT NULL");
			fprintf(traceFile_, "\n");
			return;
		}
		else
		{
			StringObject * so = (StringObject *) obj;
			const u2 *s = so->chars();
			if ( s == NULL )
        		{
        			ALOG(LOG_VERBOSE, "YWB","string is null");
        			return;
        		}
        		this->record_str( obj, s, sizeof(u2) * so->length(), flag );		
		}

	}

	void ObjTracer::extract_other(const Object * const obj)
	{

		fprintf( traceFile_, "instUid %u #%d:  %p\n%s\n", 
			opcodeTracer.get_instUid(), OTHER_OBJ, obj, obj->clazz->descriptor);

		fflush(traceFile_);
	}

	void ObjTracer::extract_location(const Object * const obj)
    	{
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), LOCAT_OBJ, obj);

		//longitude
		InstField *pF = &obj->clazz->ifields[6];
		union myunion m;
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//latitude
		pF = &obj->clazz->ifields[9];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//lng2 in cache
		pF = &obj->clazz->ifields[7];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//lat2 in cache
		pF = &obj->clazz->ifields[11];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//lng1 in cache
		pF = &obj->clazz->ifields[8];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//lat1 in cache
		pF = &obj->clazz->ifields[10];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u);

		//provider: GPS or Network
		pF = &obj->clazz->ifields[1];
		Object *o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );

		//fprintf(f, "\n");
		fflush(traceFile_);
    }

    void ObjTracer::extract_swapTestClass( const Object * const obj )
	{
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), TEST_OBJ, obj );

		InstField *pF = &obj->clazz->ifields[6];
		u4 i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i );

		pF = &obj->clazz->ifields[4];
		union myunion m;
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u );

		//fprintf(f, "\n");
		fflush(traceFile_);

	}

	void ObjTracer::extract_activRecord( const Object * const obj )
	{
		InstField *pF, *pF1;
		Object *o, *o1;
		
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), ACTREC_OBJ, obj );

		//baseDir
		pF = &obj->clazz->ifields[2];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );

		//taskAffinity
		pF = &obj->clazz->ifields[6];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//dataDir
		pF = &obj->clazz->ifields[10];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//stringName
		pF = &obj->clazz->ifields[11];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//shortComponentName
		pF = &obj->clazz->ifields[14];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//resDir
		pF = &obj->clazz->ifields[21];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//realActivity
		pF = &obj->clazz->ifields[22];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		if ( check_obj(o) )
		{
			//mClass in mComponent
			pF1 = &o->clazz->ifields[0];
			o1 = dvmGetFieldObject ( o, pF1->byteOffset );
			this->extract_str(o1);
			//mPackage in mComponent
			pF1 =  &o->clazz->ifields[1];
			o1 = dvmGetFieldObject ( o, pF1->byteOffset );
			this->extract_str(o1);
		}
		else
			fprintf(traceFile_, "\n\n");
		
		//processName
		pF = &obj->clazz->ifields[23];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );
		
		//intent
		pF = &obj->clazz->ifields[25];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		if (check_obj(o))
		{
			pF1 = &o->clazz->ifields[0];
			o1 = dvmGetFieldObject( o, pF1->byteOffset );
			this->extract_str(o1);
		}
		else
			fprintf(traceFile_, "\n");
		
		//packageName	
		pF = &obj->clazz->ifields[28];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o );	
		
		//launchedFromUid
		pF = &obj->clazz->ifields[61];
		u4 i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i );

		//fprintf(f, "\n");
		fflush(traceFile_);
	}


	void ObjTracer::extract_intent( Object * obj )
	{	
		InstField *pF1, *pF2;
		Object *o1, *o2;
		
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), INTENT_OBJ, obj );

			
		// mAction in Intent
		pF1 = &obj->clazz->ifields[0];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		this->extract_str(o1);
		
		//mComponent in Intent
		pF1 = &obj->clazz->ifields[3];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		{
			if (check_obj(o1))
			{
				//mClass in mComponent
				pF2 = &o1->clazz->ifields[0];
				o2 = dvmGetFieldObject ( o1, pF2->byteOffset );
				this->extract_str(o2);
				//mPackage in mComponent
				pF2 =  &o1->clazz->ifields[1];
				o2 = dvmGetFieldObject ( o1, pF2->byteOffset );
				this->extract_str(o2);
			}
			else
				fprintf(traceFile_, "\n\n");
			
		}

		//mType in Intent
		pF1 = &obj->clazz->ifields[6];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		this->extract_str(o1);
		
		//mPackage in Intent
		pF1 = &obj->clazz->ifields[7];
		o1 = dvmGetFieldObject( obj, pF1->byteOffset );
		this->extract_str(o1);

		//fprintf(f, "\n");
		fflush(traceFile_);				
	}

	void ObjTracer::extract_procRecord( const Object* const obj )
	{
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), PROREC_OBJ, obj );

//processName
		InstField* pF = &obj->clazz->ifields[20];
		Object* o = dvmGetFieldObject( obj, pF->byteOffset );
		this->extract_str(o);
//pid
		pF = &obj->clazz->ifields[58];
		u4 i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i );
//uid
		pF = &obj->clazz->ifields[77];
		i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i );

		//fprintf(f, "\n");
		fflush(traceFile_);
	}

	void ObjTracer::extract_compName( const Object * const obj )
	{
		InstField* pF;
		Object* o;

		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), CMPNM_OBJ, obj );

		for ( int i = 0; i < 2; i++ )
		{
			pF = &obj->clazz->ifields[i];
			o = dvmGetFieldObject( obj, pF->byteOffset );
			this->extract_str(o);
		}

		//fprintf(f, "\n");
		fflush(traceFile_);

	}

	void ObjTracer::extract_activInfo( const Object* const obj )
	{
		InstField* pF;
		ClassObject * clazz  = obj->clazz;
		Object* o;
	
		fprintf( traceFile_, "instUid %u #%d:  %p\n", 
			opcodeTracer.get_instUid(), CMPNM_OBJ, obj );

		//taskAffinity in ActivityInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o );

		clazz = clazz->super;
		//processName in ComponentInfo
		pF = &clazz->ifields[1];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o );

		clazz = clazz->super;
		//packageName in PackageItemInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o );

		//name in PackageItemInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o );

		//fprintf(f, "\n");
		fflush(traceFile_);

	}

	void ObjTracer::modify_intent( Object * obj)
	{
		//try to clear the action
		//action
		InstField *pF = &obj->clazz->ifields[0];
		//dvmDumpObject(obj);
		ALOG(LOG_VERBOSE,"YWB", "set action null");
		dvmSetFieldObject( obj, pF->byteOffset, NULL);
	}

}
