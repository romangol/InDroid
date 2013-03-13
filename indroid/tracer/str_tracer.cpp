#include "indroid/tracer.h"

#include <wchar.h>
#include <cstdio>
#include <fstream>

//#include <sys/stat.h>
//#include <sys/types.h>
//#include <signal.h>

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
using std::set;
using std::ifstream;
	
namespace gossip_loccs
{
	bool Tracer::init_recordFlag()
	{
		// get flag.dlist path
		char name[MaxLineLen] = {0};
		snprintf ( name, MaxLineLen, "%s/flag.dlist", apkDir_.c_str() );

		//make recordFlag
		recordFlag_ = 0;
		char t = 0;
		ifstream f(name);

		for (int i = 0; i < 4 && !f.eof(); i++)
		{
			f >> t;
			if ( t == '1' )
				recordFlag_ += 1 << i;
		}

		GOSSIP( "Init recordFlag: %d\n", recordFlag_ );
		
		return true;

	}

	void Tracer::record_str( const u2 * const str, size_t len, ObjWriteMode flag )
	{
		const unsigned char n = '\n' ;
		const unsigned char space = ' ';
		FILE * f;
		if ( flag == FUNC_OBJ || flag == FUNC_STR )
			f = fpFuncs_;
		else if ( flag == OPC_OBJ || flag == OPC_STR )
			f = fpObj_;
		else
		{
			GOSSIP ( "error in which file to write" );
			return;
		}

		if ( flag == OPC_STR )
		{
			fprintf( f, "instUid %u", this->instUid_);
		}
		if ( flag == OPC_STR || flag == FUNC_STR )
			fprintf(f, " #%d: ", STR_OBJ);


		if ( len >= StrMaxLen )
			len = StrMaxLen;
		fwrite( &space, 1, 1, f );
		//fwrite( &len, sizeof(size_t), 1, f );
		fwrite( str, 1, len, f );
		//if ( flag == OPC_STR || FUNC_STR )
		fwrite( &n, 1, 1, f );
		fflush( f );
	}

	bool Tracer::check_obj (  const Object * const obj )
	{
		if ( obj == NULL )
			return false;
		if ( (u4)(obj->clazz) <= 65536 || obj->clazz == NULL)
			return false;
		if ( obj->clazz->descriptor == NULL)
			return false;
		return true;
	}



	void Tracer::monitor_obj ( Object * obj)
	{
		if ( !(recordFlag_ & 0x01) )
			return ;
		this->record_obj( obj, TO_OPC);
	}

	void Tracer::record_obj ( Object * obj, ObjWriteMode flag )
	{
		// first check if the object is null
		if ( !check_obj ( obj ) )
			return;
		
			
		// then acquire the type from obj->clazz->descriptor
		u4 hash = BKDRHash (obj->clazz->descriptor);

		// whether the type should be monitored
		//if ( objectFilter_.find( hash ) == objectFilter_.end() )
		{	
			if (flag == TO_FUNC)
			{
				fprintf(fpFuncs_, "\n");
				fflush(fpFuncs_);
			}	
			return;
		}

		//else if ( hash == BKDRHash( "Lcom/android/server/am/ActivityRecord;" ) )
		if ( hash == BKDRHash( "Lcom/android/server/am/ActivityRecord;" ) )
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_activRecord( obj, flag );

		}	

		else if ( hash == BKDRHash( "Lcom/android/server/am/ProcessRecord;") )
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_procRecord( obj, flag );
		}


		else if ( hash == BKDRHash( "Landroid/content/pm/ActivityInfo;"))
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_activInfo( obj, flag );
		}

		// monitor information
		else if ( hash == BKDRHash( "Landroid/content/Intent;") ) 
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_intent( obj, flag );

			//try to modify the intent
			
			if ( 0 == strcmp(apkDir_.c_str(),"/data/data/com.example.testsms") &&  flag == FUNC_OBJ )
			{
				this->modify_intent( obj );
			}
			

		}
		
		else if ( hash == BKDRHash( "Ljava/lang/String;") )
		{
			change_flag(flag, false);
			if ( flag == ERROR )
				return ;
			this->extract_str( obj, flag );
		}

		else if ( hash == BKDRHash( "Landroid/content/ComponentName;") ) 
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_compName( obj, flag );
		}

		else if ( hash == BKDRHash( "Landroid/location/Location;") ) 
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;

			this->extract_location(obj, flag);
		}

		//just for test!!!!!
		else if ( hash == BKDRHash( "Lcom/swapTest/SwapTestActivity$TestClass;") )
		{
			change_flag(flag);
			if ( flag == ERROR )
				return ;
          
			this->extract_swapTestClass(obj, flag);
		}

	}


	void Tracer::change_flag(ObjWriteMode &flag, bool b )
	{
		//true is for obj, false is for str
		if ( flag == TO_FUNC && b )
			flag = FUNC_OBJ;
		else if ( flag == TO_OPC && b )
			flag = OPC_OBJ;
		else if ( flag == TO_FUNC && !b )
			flag = FUNC_STR;
		else if ( flag == TO_OPC && !b )
			flag = OPC_STR;
		else
		{
			flag = ERROR;
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
		}	

	}

    void Tracer::extract_location(const Object * const obj, ObjWriteMode flag)
    {
    	FILE *f;

    	if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf( f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}

		fprintf(f, " #%d: ", LOCAT_OBJ);


		//longitude
		InstField *pF = &obj->clazz->ifields[6];
		union myunion m;
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//latitude
		pF = &obj->clazz->ifields[9];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//lng2 in cache
		pF = &obj->clazz->ifields[7];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//lat2 in cache
		pF = &obj->clazz->ifields[11];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//lng1 in cache
		pF = &obj->clazz->ifields[8];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//lat1 in cache
		pF = &obj->clazz->ifields[10];
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//provider: GPS or Network
		pF = &obj->clazz->ifields[1];
		Object *o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );

		//fprintf(f, "\n");
		fflush(f);

    }
    //just for test!!!!
	void Tracer::extract_swapTestClass( const Object * const obj, ObjWriteMode flag )
	{
		FILE *f;
		InstField *pF;
		u4 i;

		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf( f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}

		fprintf(f, " #%d: ", TEST_OBJ);

		pF = &obj->clazz->ifields[6];
		i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i, flag);

		pF = &obj->clazz->ifields[4];
		union myunion m;
		m.d = dvmGetFieldDouble( obj, pF->byteOffset );
		this->record_normal('D', m.u, flag);

		//fprintf(f, "\n");
		fflush(f);

	}


	void Tracer::extract_activRecord( const Object * const obj, ObjWriteMode flag )
	{
		InstField *pF, *pF1;
		Object *o, *o1;
		FILE *f;
		u4 i;
		
		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf( f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}	

		fprintf(f, " #%d: ", ACTREC_OBJ);

		//baseDir
		pF = &obj->clazz->ifields[2];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );

		//taskAffinity
		pF = &obj->clazz->ifields[6];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//dataDir
		pF = &obj->clazz->ifields[10];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//stringName
		pF = &obj->clazz->ifields[11];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//shortComponentName
		pF = &obj->clazz->ifields[14];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//resDir
		pF = &obj->clazz->ifields[21];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//realActivity
		pF = &obj->clazz->ifields[22];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		if (check_obj(o))
		{
			//mClass in mComponent
			pF1 = &o->clazz->ifields[0];
			o1 = dvmGetFieldObject ( o, pF1->byteOffset );
			this->extract_str(o1, flag);
			//mPackage in mComponent
			pF1 =  &o->clazz->ifields[1];
			o1 = dvmGetFieldObject ( o, pF1->byteOffset );
			this->extract_str(o1, flag);
		}
		else
			fprintf(f, "\n\n");
		
		//processName
		pF = &obj->clazz->ifields[23];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );
		
		//intent
		pF = &obj->clazz->ifields[25];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		if (check_obj(o))
		{
			pF1 = &o->clazz->ifields[0];
			o1 = dvmGetFieldObject( o, pF1->byteOffset );
			this->extract_str(o1, flag);
		}
		else
			fprintf(f, "\n");
		
		//packageName	
		pF = &obj->clazz->ifields[28];
		o = dvmGetFieldObject ( obj, pF->byteOffset );
		this->extract_str( o, flag );	
		
		//launchedFromUid
		pF = &obj->clazz->ifields[61];
		i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i, flag );

		//fprintf(f, "\n");
		fflush(f);

	}


	void Tracer::extract_intent( Object * obj, ObjWriteMode flag)
	{
		//ALOG(LOG_VERBOSE,"YWB","EXTRACT INTENT");
		//dvmDumpObject(obj);
		
		InstField *pF1, *pF2;
		Object *o1, *o2;
		FILE *f;
		
		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf( f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}

		fprintf(f, " #%d: ", INTENT_OBJ);
			
		// mAction in Intent
		pF1 = &obj->clazz->ifields[0];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		this->extract_str(o1, flag);
		
		//mComponent in Intent
		pF1 = &obj->clazz->ifields[3];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		{
			if (check_obj(o1))
			{
				//mClass in mComponent
				pF2 = &o1->clazz->ifields[0];
				o2 = dvmGetFieldObject ( o1, pF2->byteOffset );
				this->extract_str(o2, flag);
				//mPackage in mComponent
				pF2 =  &o1->clazz->ifields[1];
				o2 = dvmGetFieldObject ( o1, pF2->byteOffset );
				this->extract_str(o2, flag);
			}
			else
				fprintf(f, "\n\n");
			
		}

		//mType in Intent
		pF1 = &obj->clazz->ifields[6];
		o1 = dvmGetFieldObject ( obj, pF1->byteOffset );
		this->extract_str(o1, flag);
		
		//mPackage in Intent
		pF1 = &obj->clazz->ifields[7];
		o1 = dvmGetFieldObject( obj, pF1->byteOffset );
		this->extract_str(o1,flag);

		//fprintf(f, "\n");
		fflush(f);
				
	}

	void Tracer::extract_procRecord( const Object* const obj, ObjWriteMode flag )
	{
		InstField* pF;
		Object* o;
		FILE *f;

		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf(f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}

		fprintf(f, " #%d: ", PROREC_OBJ);

//processName
		pF = &obj->clazz->ifields[20];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this->extract_str(o, flag);
//pid
		pF = &obj->clazz->ifields[58];
		u4 i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i, flag );
//uid
		pF = &obj->clazz->ifields[77];
		i = dvmGetFieldInt( obj, pF->byteOffset );
		this->record_normal('I', &i, flag );

		//fprintf(f, "\n");
		fflush(f);

	}

	void Tracer::extract_compName( const Object * const obj, ObjWriteMode flag )
	{
		InstField* pF;
		Object* o;
		FILE *f;

		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf(f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}	
		
		fprintf(f, " #%d: ", CMPNM_OBJ);
		//mclass and mPackage
		for ( int i = 0; i < 2; i++ )
		{
			pF = &obj->clazz->ifields[i];
			o = dvmGetFieldObject( obj, pF->byteOffset );
			this->extract_str(o, flag);
		}

		//fprintf(f, "\n");
		fflush(f);

	}

	void Tracer::extract_activInfo( const Object* const obj, ObjWriteMode flag )
	{
		InstField* pF;
		ClassObject * clazz  = obj->clazz;
		Object* o;
		FILE *f;
	
		if ( flag == FUNC_OBJ )
			f = fpFuncs_;
		else if (flag == OPC_OBJ )
		{
			f = fpObj_;
			fprintf(f, "instUid %u", this->instUid_);
		}
		else
		{
			ALOG( LOG_VERBOSE, "YWB", "error in which file to write");
			return;
		}

		fprintf(f, " #%d: ", ACTINFO_OBJ);

		//taskAffinity in ActivityInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o, flag );

		clazz = clazz->super;
		//processName in ComponentInfo
		pF = &clazz->ifields[1];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o, flag );

		clazz = clazz->super;
		//packageName in PackageItemInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o, flag );

		//name in PackageItemInfo
		pF = &clazz->ifields[0];
		o = dvmGetFieldObject( obj, pF->byteOffset );
		this -> extract_str( o, flag );

		//fprintf(f, "\n");
		fflush(f);

	}


	void Tracer::extract_str (Object * obj, ObjWriteMode flag)
	{
		if ( !check_obj ( obj ) )
		{
			FILE *f;
			ALOG(LOG_VERBOSE, "YWB", "OBJECT NULL");
			if (flag == OPC_OBJ || flag == OPC_STR)
				f = fpObj_;
			else if (flag == FUNC_STR || flag == FUNC_OBJ)
				f = fpFuncs_;
			else
			{
				ALOG ( LOG_VERBOSE, "YWB", "error in which file to write" );
				return;
			}
			fprintf(f, "\n");
			fflush(f);
			return;
		}

/*
		if ( strcmp (obj->clazz->descriptor, "Ljava/lang/String;") != 0)
		{
			ALOG(LOG_VERBOSE,"YWB","NOT STRING");
			return;
		}	
*/		
		{			
			StringObject * so = (StringObject *) obj;
			const u2 *s = so->chars();
			if ( s == NULL )
        	{
        		ALOG(LOG_VERBOSE, "YWB","string is null");
        		return;
        	}
        	this->record_str( s, sizeof(u2) * so->length(), flag );
		}
	}

	void Tracer::modify_intent( Object * obj)
	{
		//try to clear the action
		//action
		InstField *pF = &obj->clazz->ifields[0];
		//dvmDumpObject(obj);
		ALOG(LOG_VERBOSE,"YWB", "set action null");
		dvmSetFieldObject( obj, pF->byteOffset, NULL);

	}

	void Tracer::dump_obj( const Object * const obj )
	{
		if ( !check_obj ( obj ) )
		{
			ALOG( LOG_VERBOSE, "YWB", "NULL OBJECT" );
			return;
		}
		ClassObject* clazz = obj->clazz;
/*
		u4 hash = BKDRHash ( clazz->descriptor );
		if ( objectFilter_.find( hash ) == objectFilter_.end() )
			return;
*/
		ALOG( LOG_VERBOSE, "YWB", " class --%s ", clazz->descriptor );
		for ( int i = 0; i < clazz->ifieldCount; i++ )
		{
			const InstField* pField = &clazz->ifields[i];
			char type = pField->signature[0];
				
			if ( type == 'F' || type == 'D' )
			{
				double dval;
				if ( type == 'F' )
					dval = dvmGetFieldFloat( obj, pField->byteOffset );
				else
					dval = dvmGetFieldDouble( obj, pField->byteOffset );
				ALOG( LOG_VERBOSE, "YWB", " %2d: %s, %s, %lf ", i, pField->name, pField->signature, dval );
			}
			else
			{
				u8 lval;
				if ( type == 'J' )
					lval = dvmGetFieldLong( obj, pField->byteOffset );
				else if ( type == 'Z' )
					lval = dvmGetFieldBoolean( obj, pField->byteOffset );
				else if ( type == 'L' )
				{
					Object* o = dvmGetFieldObject ( obj, pField->byteOffset );
					dump_obj ( o );
				}
				else
					lval = dvmGetFieldInt( obj, pField->byteOffset );
				//ALOG( LOG_VERBOSE, "YWB", " %2d: %s, %s, 0x%08llx ", i, pField->name, pField->signature, lval );
			}
		}
		
	}

}; // end of namespace gossip_loccs

