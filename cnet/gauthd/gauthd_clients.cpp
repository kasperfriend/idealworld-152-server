//
// Link-time glue for gauthd.
//
// gauthd's generated rpc stubs (stubs.cxx -> userlogin2.hrp, matrixpasswd.hrp,
// matrixpasswd2.hrp, matrixtoken.hrp) forward the requests they cannot serve
// themselves through GNET::GAuthClient / GNET::GameDBClient, whose declarations
// live with gdeliveryd.  Those two classes are only ever *used* by a daemon that
// keeps an outbound connection to gauthd / gamedbd, so gauthd itself needs the
// bare minimum: the singletons (their vtables are emitted with them) and the
// members the generated code calls.
//
// Pulling in gdeliveryd's gauthclient.cpp / gamedbclient.cpp is not an option:
// they reference GDeliveryServer::instance, WebTradeMarket, BattleManager, ... -
// i.e. most of gdeliveryd.  So the forwarding paths stay inert here, exactly as
// they always were in this daemon: SendProtocol() reports "not delivered", which
// makes the callers in the *.hrp templates answer with ERR_DELIVER_SEND, and the
// session callbacks do nothing because gauthd never opens these managers.
//

#include "gauthclient.hpp"
#include "gamedbclient.hpp"
#include "state.hxx"

namespace GNET
{

GAuthClient GAuthClient::instance;
GameDBClient GameDBClient::instance;

void GAuthClient::Reconnect()
{
}

const GAuthClient::Session::State * GAuthClient::GetInitState() const
{
	return &state_GAuthServer;
}

void GAuthClient::OnAddSession(Session::ID)
{
}

void GAuthClient::OnDelSession(Session::ID)
{
}

void GAuthClient::OnAbortSession(Session::ID)
{
}

void GAuthClient::OnCheckAddress(SockAddr &) const
{
}

void GAuthClient::OnSetTransport(Session::ID, const SockAddr &, const SockAddr &)
{
}

void GAuthClient::IdentifyFailed()
{
}

bool GAuthClient::SendProtocol(const Protocol *protocol)
{
	return conn_state && Send(sid, protocol);
}

bool GAuthClient::SendProtocol(Protocol *protocol)
{
	return conn_state && Send(sid, protocol);
}

void GameDBClient::Reconnect()
{
}

const GameDBClient::Session::State * GameDBClient::GetInitState() const
{
	return &state_GAuthServer;
}

void GameDBClient::OnAddSession(Session::ID)
{
}

void GameDBClient::OnDelSession(Session::ID)
{
}

void GameDBClient::OnAbortSession(Session::ID)
{
}

void GameDBClient::OnCheckAddress(SockAddr &) const
{
}

};
