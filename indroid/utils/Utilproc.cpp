#include <fstream>

#include "indroid/utils/Utilproc.h"
#include "indroid/Constant.h"

// define YWB message output macro
#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "YWB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

using std::string;
using std::map;
using std::ifstream;

namespace gossip_loccs
{
	UtilProc::UtilProc()
	{
		if ( !this->init_uidmap() )
			GOSSIP ( "uidmap init process failure" );
	}

	bool UtilProc::init_uidmap()
	{
		ifstream f ("/data/system/packages.list");
		if ( !f ) return false;	

		// read a line and get path
		string s;
		string dir;
		u4 d;
		u4 e;

		while ( !f.eof() )
		{
			f >> s >> d >> e >> dir; // ugly... 
			uidMap_[d] = dir;
		}

		/*
		if ( !f.eof() )
		{
			return false;
		}
		*/

		GOSSIP( "Init map items: %d\n", uidMap_.size() );
		return true;	
	}

	const std::string UtilProc::get_proc_name ()
	{
		// query other process is forbidden
		u4 pid = getpid();
		string s;

		// Get procFile's name from pid
	    char name[ProcFileNameMaxLen];
	    snprintf( name,	ProcFileNameMaxLen, "/proc/%d/cmdline", pid );
		ifstream f( name );
		if ( !f )
		{
			GOSSIP( "GET PROCESS NAME ERROR\n" );
			return s;
		}
		
		std::getline(f, s);

		return s;
	}

	const std::string UtilProc::get_apk_dir ()
	{
		// don't allow to query other process' uid
		u4 uid = getuid();

		map<u4, string>::const_iterator it = uidMap_.find(uid);

		if ( it != uidMap_.end() )
			return it->second; 
		return string();
	}

	bool UtilProc::apk_should_be_traced ()
	{
		//try to find system_server,uid :1000, pid is not solid but around 150
		/*
		if (uid == 1000)
			kill(getpid(),SIGKILL);*/

		// get class.dlist path
		char classFilterFilename[MaxLineLen] = {0};
		snprintf ( classFilterFilename, MaxLineLen, "%s/class.dlist", this->get_apk_dir().c_str() );

		// access(filename, 0) == 0 means file do exist.
		return ( access( classFilterFilename, 0 ) == 0 );
	}

}
// end of namespace loccs


