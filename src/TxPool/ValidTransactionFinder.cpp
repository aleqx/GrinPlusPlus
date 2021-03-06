#include "ValidTransactionFinder.h"
#include "TransactionAggregator.h"

#include <Core/Validation/TransactionValidator.h>
#include <Core/Validation/KernelSumValidator.h>
#include <Common/Util/FunctionalUtil.h>
#include <PMMR/TxHashSetManager.h>
#include <Database/BlockDb.h>

ValidTransactionFinder::ValidTransactionFinder()
{

}

std::vector<Transaction> ValidTransactionFinder::FindValidTransactions(
	std::shared_ptr<const IBlockDB> pBlockDB,
	ITxHashSetConstPtr pTxHashSet,
	const std::vector<Transaction>& transactions,
	const std::unique_ptr<Transaction>& pExtraTransaction) const
{
	std::vector<Transaction> validTransactions;
	for (const Transaction& transaction : transactions)
	{
		std::vector<Transaction> candidateTransactions = validTransactions;
		if (pExtraTransaction != nullptr)
		{
			candidateTransactions.push_back(*pExtraTransaction);
		}

		candidateTransactions.push_back(transaction);

		// Build a single aggregate tx from candidate txs.
		Transaction aggregateTransaction = TransactionAggregator::Aggregate(candidateTransactions);

		// We know the tx is valid if the entire aggregate tx is valid.
		if (IsValidTransaction(pBlockDB, pTxHashSet, aggregateTransaction))
		{
			validTransactions.push_back(transaction);
		}
	}

	return validTransactions;
}

bool ValidTransactionFinder::IsValidTransaction(std::shared_ptr<const IBlockDB> pBlockDB, ITxHashSetConstPtr pTxHashSet, const Transaction& transaction) const
{
	try
	{
		TransactionValidator().Validate(transaction);

		// Validate the tx against current chain state.
		// Check all inputs are in the current UTXO set.
		// Check all outputs are unique in current UTXO set.
		if (!pTxHashSet->IsValid(pBlockDB, transaction))
		{
			return false;
		}
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}