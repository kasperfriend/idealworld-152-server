
#ifndef __GNET_CERTFINISH_HPP
#define __GNET_CERTFINISH_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"


namespace GNET
{

class CertFinish : public GNET::Protocol
{
	#include "certfinish"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		/* protocol handle */
		GDeliveryServer * dsm = GDeliveryServer::GetInstance();
		GNET::Log::log(LOG_INFO,"=====end CertVerify:receive CertFinish from au,aid=%d,zoneid=%d,sid=%d",dsm->aid,dsm->zoneid,sid);
		GAuthClient *acm = GAuthClient::GetInstance();
		acm->bl_keepalive = true;
		acm->SetOSecurity(sid,GNET::ARCFOURSECURITY,acm->osec_key);	
		AnnounceZoneid3 ann((unsigned char)dsm->zoneid, dsm->aid, acm->GetBlreset(), 0,0,0,1,0);

		if(acm->SendProtocol(ann))
		{
			GNET::Log::log(LOG_INFO,"send AnnounceZoneid3 to au ok,aid=%d,zoneid=%d",ann.aid,ann.zoneid);
		}
		else
			GNET::Log::log(LOG_WARNING,"send AnnounceZoneid3 to au failed,aid=%d,zoneid=%d",ann.aid,ann.zoneid);
		if(acm->GetBlreset()) acm->SetBlreset(false);
	}
};

};

#endif
