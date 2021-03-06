#pragma once

#include "../ConnectionManager.h"

#include <Crypto/Hash.h>
#include <Net/Socket.h>
#include <TxPool/PoolType.h>
#include <Core/Models/FullBlock.h>
#include <BlockChain/BlockChainServer.h>
#include <Common/ConcurrentQueue.h>
#include <string>
#include <cstdint>
#include <atomic>
#include <thread>

// Forward Declarations
class Config;
class TxHashSetArchiveMessage;
class Transaction;

class BlockPipe
{
public:
	static std::shared_ptr<BlockPipe> Create(
		const Config& config,
		ConnectionManagerPtr pConnectionManager,
		IBlockChainServerPtr pBlockChainServer
	);
	~BlockPipe();

	bool AddBlockToProcess(const uint64_t connectionId, const FullBlock& block);
	bool IsProcessingBlock(const Hash& hash) const;

private:
	BlockPipe(const Config& config, ConnectionManagerPtr pConnectionManager, IBlockChainServerPtr pBlockChainServer);

	const Config& m_config;
	ConnectionManagerPtr m_pConnectionManager;
	IBlockChainServerPtr m_pBlockChainServer;

	struct BlockEntry
	{
		BlockEntry(const uint64_t connId, const FullBlock& fullBlock)
			: connectionId(connId), block(fullBlock)
		{

		}

		uint64_t connectionId;
		FullBlock block;
	};

	// Pre-Process New Blocks
	static void Thread_ProcessNewBlocks(BlockPipe& pipeline);
	static void ProcessNewBlock(BlockPipe& pipeline, const BlockEntry& blockEntry);
	std::thread m_blockThread;
	ConcurrentQueue<BlockEntry> m_blocksToProcess;

	// Process Next Block
	std::thread m_processThread;
	static void Thread_PostProcessBlocks(BlockPipe& pipeline);

	std::atomic_bool m_terminate;
};