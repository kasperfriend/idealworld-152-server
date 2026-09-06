
#ifndef __GNET_ANNOUNCEZONEID3_HPP
#define __GNET_ANNOUNCEZONEID3_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"

#include "gauthserver.hpp"
namespace GNET
{

// gdeliveryd announces the zone with AnnounceZoneid3 (PROTOCOL 527) unless it
// is built/configured for certificate auth; gauthd used to know only the old
// AnnounceZoneid (505), rejected the newer message with "Protocol Unknown"
// and dropped the session, making gdeliveryd reconnect forever.  Accept it
// here and record the zone like the old variant does.
class AnnounceZoneid3 : public GNET::Protocol
{
	#include "announcezoneid3"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		// TODO
		Thread::Mutex::Scoped l(GAuthServer::GetInstance()->locker_zonemap);
		GAuthServer::GetInstance()->zonemap[sid] = (char)zoneid;
		DEBUG_PRINT("gauthd::annoucezoneid3: zoneid=%d,aid=%d belongs to session %d\n", zoneid, aid, sid);
	}
};

};

#endif
