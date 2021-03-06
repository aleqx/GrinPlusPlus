#pragma once

#include <Core/Models/FullBlock.h>
#include <Core/Models/BlockSums.h>
#include <Database/BlockDb.h>
#include <Core/Traits/Lockable.h>
#include <PMMR/TxHashSet.h>
#include <memory>

// Forward Declarations
class BlindingFactor;

class BlockValidator
{
public:
	BlockValidator(std::shared_ptr<const IBlockDB> pBlockDB, ITxHashSetConstPtr pTxHashSet);

	BlockSums ValidateBlock(const FullBlock& block) const;
	void VerifySelfConsistent(const FullBlock& block) const;

private:
	void VerifyBody(const FullBlock& block) const;
	void VerifyKernelLockHeights(const FullBlock& block) const;
	bool IsCoinbaseValid(const FullBlock& block) const;

	std::shared_ptr<const IBlockDB> m_pBlockDB;
	ITxHashSetConstPtr m_pTxHashSet;
};