#pragma once

#include "Chain.h"

#include <Config/Config.h>
#include <Core/Traits/Lockable.h>

class ChainStore : public Traits::IBatchable
{
public:
	static std::shared_ptr<Locked<ChainStore>> Load(const Config& config, std::shared_ptr<BlockIndex>);

	virtual void Commit() override final;
	virtual void Rollback() override final;
	virtual void OnInitWrite() override final;
	virtual void OnEndWrite() override final;

	std::shared_ptr<Chain> GetChain(const EChainType chainType);
	std::shared_ptr<const Chain> GetChain(const EChainType chainType) const;
	//std::shared_ptr<BlockIndex> GetOrCreateIndex(const Hash& hash, const uint64_t height);
	std::shared_ptr<const BlockIndex> FindCommonIndex(const EChainType chainType1, const EChainType chainType2) const;

	//
	// Applies all of the blocks from the source chain to the destination chain, up to the specified height.
	//
	void ReorgChain(const EChainType source, const EChainType destination, const uint64_t height);
	void ReorgChain(const EChainType source, const EChainType destination);

	void AddBlock(const EChainType source, const EChainType destination, const uint64_t height);

	std::shared_ptr<Chain> GetConfirmedChain() { return m_pConfirmedChain; }
	std::shared_ptr<Chain> GetCandidateChain() { return m_pCandidateChain; }
	std::shared_ptr<Chain> GetSyncChain() { return m_pSyncChain; }

	std::shared_ptr<const Chain> GetConfirmedChain() const { return m_pConfirmedChain; }
	std::shared_ptr<const Chain> GetCandidateChain() const { return m_pCandidateChain; }
	std::shared_ptr<const Chain> GetSyncChain() const { return m_pSyncChain; }

private:
	ChainStore(std::shared_ptr<Chain> pConfirmedChain, std::shared_ptr<Chain> pCandidateChain, std::shared_ptr<Chain> pSyncChain);

	std::shared_ptr<Chain> m_pConfirmedChain;
	std::shared_ptr<Chain> m_pCandidateChain;
	std::shared_ptr<Chain> m_pSyncChain;
};