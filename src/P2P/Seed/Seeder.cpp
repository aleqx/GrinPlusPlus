#include "Seeder.h"
#include "DNSSeeder.h"
#include "PeerManager.h"
#include "../ConnectionManager.h"
#include "../Messages/GetPeerAddressesMessage.h"

#include <P2P/Capabilities.h>
#include <Net/SocketAddress.h>
#include <Net/Socket.h>
#include <BlockChain/BlockChainServer.h>
#include <Common/Util/StringUtil.h>
#include <Common/Util/ThreadUtil.h>
#include <Infrastructure/ThreadManager.h>
#include <Infrastructure/Logger.h>
#include <algorithm>

Seeder::Seeder(
	const Config& config,
	ConnectionManager& connectionManager,
	Locked<PeerManager> peerManager,
	IBlockChainServerPtr pBlockChainServer,
	std::shared_ptr<Pipeline> pPipeline,
	SyncStatusConstPtr pSyncStatus)
	: m_config(config),
	m_connectionManager(connectionManager),
	m_peerManager(peerManager),
	m_pBlockChainServer(pBlockChainServer),
	m_pPipeline(pPipeline),
	m_pSyncStatus(pSyncStatus),
	m_pContext(std::make_shared<asio::io_context>()),
	m_terminate(false)
{

}

Seeder::~Seeder()
{
	m_terminate = true;
	ThreadUtil::Join(m_listenerThread);
	ThreadUtil::Join(m_seedThread);
}

std::shared_ptr<Seeder> Seeder::Create(
	const Config& config,
	ConnectionManager& connectionManager,
	Locked<PeerManager> peerManager,
	IBlockChainServerPtr pBlockChainServer,
	std::shared_ptr<Pipeline> pPipeline,
	SyncStatusConstPtr pSyncStatus)
{
	std::shared_ptr<Seeder> pSeeder = std::shared_ptr<Seeder>(new Seeder(
		config,
		connectionManager,
		peerManager,
		pBlockChainServer,
		pPipeline,
		pSyncStatus
	));
	pSeeder->m_seedThread = std::thread(Thread_Seed, std::ref(*pSeeder.get()));
	pSeeder->m_listenerThread = std::thread(Thread_Listener, std::ref(*pSeeder.get()));
	return pSeeder;
}

//
// Continuously checks the number of connected peers, and connects to additional peers when the number of connections drops below the minimum.
// This function operates in its own thread.
//
void Seeder::Thread_Seed(Seeder& seeder)
{
	ThreadManagerAPI::SetCurrentThreadName("SEED");
	LoggerAPI::LogTrace("Seeder::Thread_Seed() - BEGIN");

	std::chrono::system_clock::time_point lastConnectTime = std::chrono::system_clock::now() - std::chrono::seconds(10);

	const size_t minimumConnections = seeder.m_config.GetP2PConfig().GetPreferredMinConnections();
	while (!seeder.m_terminate)
	{
		seeder.m_connectionManager.PruneConnections(true);

		auto now = std::chrono::system_clock::now();
		const size_t numOutbound = seeder.m_connectionManager.GetNumOutbound();
		if (numOutbound < minimumConnections && lastConnectTime + std::chrono::seconds(2) < now)
		{
			lastConnectTime = now;
			const size_t connectionsToAdd = (std::min)((size_t)15, (minimumConnections - numOutbound));
			for (size_t i = 0; i < connectionsToAdd; i++)
			{
				seeder.SeedNewConnection();
			}
		}

		ThreadUtil::SleepFor(std::chrono::milliseconds(250), seeder.m_terminate);
	}

	LoggerAPI::LogTrace("Seeder::Thread_Seed() - END");
}

//
// Continuously listens for new connections, and drops them if node already has maximum number of connections
// This function operates in its own thread.
//
void Seeder::Thread_Listener(Seeder& seeder)
{
	ThreadManagerAPI::SetCurrentThreadName("LISTENER");
	LoggerAPI::LogTrace("Seeder::Thread_Listener() - BEGIN");

	const uint16_t portNumber = seeder.m_config.GetEnvironment().GetP2PPort();
	asio::ip::tcp::acceptor acceptor(*seeder.m_pContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), portNumber));
	asio::error_code errorCode;
	acceptor.listen(asio::socket_base::max_listen_connections, errorCode);

	if (!errorCode)
	{
		const int maximumConnections = seeder.m_config.GetP2PConfig().GetMaxConnections();
		while (!seeder.m_terminate)
		{
			SocketPtr pSocket = SocketPtr(new Socket(SocketAddress(IPAddress(), portNumber)));
			// FUTURE: Always accept, but then send peers and immediately drop
			if (seeder.m_connectionManager.GetNumberOfActiveConnections() < maximumConnections)
			{
				const bool connectionAdded = pSocket->Accept(seeder.m_pContext, acceptor, seeder.m_terminate);
				if (connectionAdded)
				{
					ConnectionPtr pConnection = Connection::Create(
						pSocket,
						seeder.m_nextId++,
						seeder.m_config,
						seeder.m_connectionManager,
						seeder.m_peerManager,
						seeder.m_pBlockChainServer,
						ConnectedPeer(Peer(pSocket->GetSocketAddress()), EDirection::INBOUND),
						seeder.m_pPipeline,
						seeder.m_pSyncStatus
					);
				}
			}
			else
			{
				ThreadUtil::SleepFor(std::chrono::milliseconds(10), seeder.m_terminate);
			}
		}

		acceptor.cancel();
	}

	LoggerAPI::LogTrace("Seeder::Thread_Listener() - END");
}

ConnectionPtr Seeder::SeedNewConnection()
{
	std::unique_ptr<Peer> pPeer = m_peerManager.Read()->GetNewPeer(Capabilities::FAST_SYNC_NODE);
	if (pPeer != nullptr)
	{
		ConnectedPeer connectedPeer(*pPeer, EDirection::OUTBOUND);
		SocketAddress address(pPeer->GetIPAddress(), m_config.GetEnvironment().GetP2PPort());
		SocketPtr pSocket = SocketPtr(new Socket(std::move(address)));
		ConnectionPtr pConnection = Connection::Create(
			pSocket,
			m_nextId++,
			m_config,
			m_connectionManager,
			m_peerManager,
			m_pBlockChainServer,
			connectedPeer,
			m_pPipeline,
			m_pSyncStatus
		);

		return pConnection;
	}
	else if (!m_usedDNS.exchange(true))
	{
		std::vector<SocketAddress> peerAddresses = DNSSeeder(m_config).GetPeersFromDNS();

		m_peerManager.Write()->AddFreshPeers(peerAddresses);
	}

	// TODO: Request new peers

	return nullptr;
}