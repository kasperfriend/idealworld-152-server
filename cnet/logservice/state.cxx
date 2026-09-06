#include "callid.hxx"

#include "protocol.h"
#include "binder.h"

namespace GNET
{

static GNET::Protocol::Type _state_LogNormal[] = 
{
	PROTOCOL_STATINFOVITAL,
	PROTOCOL_STATINFO,
	PROTOCOL_REMOTELOGVITAL,
	PROTOCOL_REMOTELOG,
};

GNET::Protocol::Manager::Session::State state_LogNormal(_state_LogNormal,
						sizeof(_state_LogNormal)/sizeof(GNET::Protocol::Type), 3600);


};

