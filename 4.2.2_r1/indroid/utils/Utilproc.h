#ifndef _DIAOS_PROCUTIL_H_
#define _DIAOS_PROCUTIL_H_

#include "indroid/Constant.h"

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "mterp/Mterp.h"

#include <map>
#include <string>

namespace gossip_loccs
{

class UtilProc
{
public:
	UtilProc();
	const std::string	get_apk_dir				();
	const std::string	get_proc_name			();
	bool 				init_uidmap				();
	bool				apk_should_be_traced	();

private:
	std::map<u4, std::string> uidMap_;
};


} // end of namespace loccs

#endif
