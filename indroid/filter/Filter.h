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
	bool method_should_be_traced	( const Method * const m );
	bool object_should_be_traced	( const Object * const obj );
	bool method_should_be_traced( const char* const cn, const char* const mn );
	bool record_should_be_opened( uint8_t c );

private:
	void		init_filter				( const std::string & n, std::set<u4> & filter );
	void 		init_recordFlag			( const std::string & n );
	std::set<u4>	list_to_hash_filter		( const char* const filename );

	std::set<u4>	classFilter_;
	std::set<u4>	methodFilter_;
	std::set<u4>	objectFilter_;
	std::string		apkDir_;
	uint8_t			recordFlag_;
};

} // end of namespace loccs

#endif
