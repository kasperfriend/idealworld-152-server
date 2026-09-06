#include "callid.hxx"

#include "protocol.h"
#include "binder.h"

namespace GNET
{

static GNET::Protocol::Type _state_LogNull[] = 
{
};

GNET::Protocol::Manager::Session::State state_LogNull(_state_LogNull,
						sizeof(_state_LogNull)/sizeof(GNET::Protocol::Type), 3600);


};

