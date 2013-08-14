#include <fstream>

#include "indroid/filter/Filter.h"

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
	void Filter::init( const std::string & apkDir )
	{
		apkDir_ = apkDir;

		string c_list("class.dlist");
		string o_list("object.dlist"); 
		string m_list("method.dlist");
		string f_list("flag.dlist");

		this->init_filter( c_list, classFilter_ );
		this->init_filter( o_list, objectFilter_ );
		this->init_filter( m_list, methodFilter_ );
		this->init_recordFlag(f_list);
	}

	void Filter::init_filter( const string & n, set<u4> & filter  )
	{
		// get class.dlist path
		char name[MaxLineLen] = {0};
		snprintf ( name, MaxLineLen, "%s/%s", apkDir_.c_str(), n.c_str() );
	
		// make class filter
		filter = list_to_hash_filter( name );

		GOSSIP( "Init %s filter items: %d\n", n.c_str(), filter.size() );
	}

	void Filter::init_recordFlag( const string &n )
	{
		char name[MaxLineLen] = {0};
		snprintf ( name, MaxLineLen, "%s/%s", apkDir_.c_str(), n.c_str() );

		char t = 0;
		recordFlag_ = 0x0;
		ifstream f(name);

		if (!f)
			recordFlag_ =  DefaultRecordFlag;

		else
		{
			for (int i = 0; i < 4 && !f.eof(); i++)
			{
				f >> t;
				if ( t == '1' )
					recordFlag_ |= 1 << i;
			}
		}

		GOSSIP( "Init recordFlag: %d", recordFlag_ );

	}

	bool Filter::record_should_be_opened( uint8_t c )
	{
		if ( recordFlag_ & c )
			return true;
		return false;
	}


	bool Filter::class_should_be_traced ( const char* const className )
	{
		if ( className[0] == 0) 
			return false;

		return classFilter_.find( BKDRHash(className) ) != classFilter_.end();
	}

	bool Filter::method_should_be_traced ( const Method * const m )
	{
		if ( m == NULL || m->clazz == NULL || m->clazz->descriptor == NULL )
			return false;

		char name[MaxLineLen] = {0};
		snprintf( name, MaxLineLen, "%s%s", m->clazz->descriptor, m->name );

		return methodFilter_.find( BKDRHash(name) ) != methodFilter_.end();
	}

	bool Filter::method_should_be_traced( const char* const cn, const char* const mn )
	{
		char n[MaxLineLen] = {0};
		snprintf( n, MaxLineLen, "%s%s", cn, mn );
		if ( n[0] == 0 )
			return false;

		return methodFilter_.find( BKDRHash(n) ) != methodFilter_.end();
	}

	bool Filter::object_should_be_traced	( const Object * const obj )
	{
		if ( obj == NULL || (u4)(obj->clazz) <= 65536 || obj->clazz == NULL || obj->clazz->descriptor == NULL )
			return false;

		return objectFilter_.find( BKDRHash(obj->clazz->descriptor) ) != objectFilter_.end();
	}

	set<u4> Filter::list_to_hash_filter( const char * const filename )
	{
		set<u4> filter;
		string s;
		ifstream f( filename );
		
		while ( std::getline(f, s) )
		{
			filter.insert( BKDRHash( s.c_str() ) );
		}
	
		return filter;
	}

} // end of namespace loccs

