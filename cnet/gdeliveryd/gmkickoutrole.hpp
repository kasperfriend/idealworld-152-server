
#ifndef __GNET_GMKICKOUTROLE_HPP
#define __GNET_GMKICKOUTROLE_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"

#include "gdeliveryserver.hpp"
#include "putroleforbid.hrp"
#include "gmkickoutrole_re.hpp"
#include "kickoutuser.hpp"
#include "forbid.hxx"
#include "mapforbid.h"
#include "mapuser.h"
namespace GNET
{

class GMKickoutRole : public GNET::Protocol
{
	#include "gmkickoutrole"

	void getHexString(char* request, int requestSize, char* result, int resultSize)
	{
	    int offset = 0;
	    
	    offset += sprintf(result+offset, "{0x");
	    for(int index = 0; index < requestSize; index++)
	    {
	        offset += sprintf(result+offset, "%02x", *(request+index));
	        if (index < (requestSize-1))
	        {
	            offset += sprintf(result+offset, ", 0x");
	        }
	    }
	    offset += sprintf(result+offset, "}");
	    
	    resultSize = offset;
	}

	void Process(Manager *manager, Manager::Session::ID sid)
	{
	    char hexReason[1024];
	    int hexReason_size = 0;
	    
    	getHexString((char*)reason.begin(), reason.size(), hexReason, hexReason_size);
    	
		LOG_TRACE("GMKickoutRole: GM=%d,roleid=%d,forbid_time=%d,reason_size=%d,reason=%s", gmroleid,
			kickroleid, forbid_time, reason.size(), (char*)hexReason);
		GRoleForbid grb(Forbid::FBD_FORBID_LOGIN,forbid_time,time(NULL),reason);
		if ( forbid_time<=0 )
			ForbidRoleLogin::GetInstance().RmvForbidRoleLogin(kickroleid);	
		else
			ForbidRoleLogin::GetInstance().SetForbidRoleLogin( kickroleid, grb );
		Thread::RWLock::RDScoped l(UserContainer::GetInstance().GetLocker());
		PlayerInfo * pinfo = UserContainer::GetInstance().FindRole(kickroleid);
		if (NULL!=pinfo && forbid_time>0)
		{
			manager->Send(pinfo->linksid,KickoutUser(pinfo->userid,pinfo->localsid,ERR_KICKOUT));
		}
		manager->Send(sid,GMKickoutRole_Re(ERR_SUCCESS,gmroleid,localsid,kickroleid));
		//put forbid login to GameDB
		RoleForbidPair arg;
		arg.key=RoleId(kickroleid);
		arg.value.add(grb);
		GameDBClient::GetInstance()->SendProtocol(Rpc::Call(RPC_PUTROLEFORBID,&arg));
	}
};

};

#endif
