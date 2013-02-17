#ifndef _DIAOS_FILTER_H_
#define _DIAOS_FILTER_H_

#include <set>
#include <string>

#include "indroid/Constant.h"

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"


namespace gossip_loccs
{

class Filter
{
public:
	void init						( const std::string & apkDir );

	bool class_should_be_traced		( const char* const className );
	bool method_should_be_traced	( const char* const className, const char* const methodName );
	bool object_should_be_traced	( const char* const objectName );

private:
	void			init_filter				( const std::string & n, std::set<u4> & filter );
	std::set<u4>	list_to_hash_filter		( const char* const filename );

	std::set<u4>	classFilter_;
	std::set<u4>	methodFilter_;
	std::set<u4>	objectFilter_;
	std::string		apkDir_;
};

} // end of namespace loccs

#endif
