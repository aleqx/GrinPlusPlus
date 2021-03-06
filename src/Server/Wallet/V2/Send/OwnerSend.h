#pragma once

#include <Config/Config.h>
#include <Wallet/WalletManager.h>
#include <civetweb/include/civetweb.h>
#include <Net/Clients/RPC/RPC.h>
#include <Net/Tor/TorConnection.h>
#include <optional>

class OwnerSend
{
public:
	OwnerSend(const Config& config, IWalletManagerPtr pWalletManager);

	RPC::Response Send(mg_connection* pConnection, RPC::Request& request);

private:
	std::shared_ptr<TorConnection> EstablishConnection(const std::optional<std::string>& addressOpt) const;
	bool CheckVersion(TorConnection& connection) const;

	const Config& m_config;
	IWalletManagerPtr m_pWalletManager;
};