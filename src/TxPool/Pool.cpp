#include "Pool.h"
#include "TransactionAggregator.h"
#include "ValidTransactionFinder.h"

#include <Common/Util/VectorUtil.h>
#include <Infrastructure/Logger.h>
#include <algorithm>
#include <unordered_map>

std::vector<Transaction> Pool::GetTransactionsByShortId(const Hash& hash, const uint64_t nonce, const std::set<ShortId>& missingShortIds) const
{
	std::vector<Transaction> transactionsFound;
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		for (const TransactionKernel& kernel : txPoolEntry.GetTransaction().GetBody().GetKernels())
		{
			const ShortId shortId = ShortId::Create(kernel.GetHash(), hash, nonce);
			if (missingShortIds.find(shortId) != missingShortIds.cend())
			{
				transactionsFound.push_back(txPoolEntry.GetTransaction());

				if (transactionsFound.size() == missingShortIds.size())
				{
					return transactionsFound;
				}
			}
		}
	}

	return transactionsFound;
}

void Pool::AddTransaction(const Transaction& transaction, const EDandelionStatus status)
{
	LOG_DEBUG("Transaction added: " + transaction.GetHash().ToHex());

	m_transactions.emplace_back(TxPoolEntry(transaction, status, std::time_t()));
}

bool Pool::ContainsTransaction(const Transaction& transaction) const
{
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		if (txPoolEntry.GetTransaction() == transaction)
		{
			return true;
		}
	}

	return false;
}

std::vector<Transaction> Pool::FindTransactionsByKernel(const std::set<TransactionKernel>& kernels) const
{
	std::set<Transaction> transactionSet;
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		for (const TransactionKernel& kernel : txPoolEntry.GetTransaction().GetBody().GetKernels())
		{
			if (kernels.count(kernel) > 0)
			{
				transactionSet.insert(txPoolEntry.GetTransaction());
			}
		}
	}

	std::vector<Transaction> transactions;
	std::copy(transactionSet.begin(), transactionSet.end(), std::back_inserter(transactions));

	return transactions;
}

std::unique_ptr<Transaction> Pool::FindTransactionByKernelHash(const Hash& kernelHash) const
{
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		for (const TransactionKernel& kernel : txPoolEntry.GetTransaction().GetBody().GetKernels())
		{
			if (kernel.GetHash() == kernelHash)
			{
				return std::make_unique<Transaction>(txPoolEntry.GetTransaction());
			}
		}
	}

	return std::unique_ptr<Transaction>(nullptr);
}

std::vector<Transaction> Pool::FindTransactionsByStatus(const EDandelionStatus status) const
{
	std::vector<Transaction> transactions;
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		if (txPoolEntry.GetStatus() == status)
		{
			transactions.push_back(txPoolEntry.GetTransaction());
		}
	}

	return transactions;
}

std::vector<Transaction> Pool::GetExpiredTransactions(const uint16_t embargoSeconds) const
{
	const std::time_t cutoff = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::seconds(embargoSeconds));

	std::vector<Transaction> transactions;
	for (const TxPoolEntry& txPoolEntry : m_transactions)
	{
		if (txPoolEntry.GetTimestamp() < cutoff)
		{
			transactions.push_back(txPoolEntry.GetTransaction());
		}
	}

	return transactions;
}

void Pool::RemoveTransaction(const Transaction& transaction)
{
	auto iter = m_transactions.begin();
	while (iter != m_transactions.end())
	{
		if (transaction == iter->GetTransaction())
		{
			m_transactions.erase(iter);
			break;
		}

		++iter;
	}
}

// Quick reconciliation step - we can evict any txs in the pool where
// inputs or kernels intersect with the block.
void Pool::ReconcileBlock(std::shared_ptr<const IBlockDB> pBlockDB, ITxHashSetConstPtr pTxHashSet, const FullBlock& block, const std::unique_ptr<Transaction>& pMemPoolAggTx)
{
	std::vector<Transaction> filteredTransactions;
	std::unordered_map<Hash, TxPoolEntry> filteredEntriesByHash;

	// Filter txs in the pool based on the latest block.
	// Reject any txs where we see a matching tx kernel in the block.
	// Also reject any txs where we see a conflicting tx,
	// where an input is spent in a different tx.
	for (auto& txPoolEntry : m_transactions)
	{
		if (!ShouldEvict(txPoolEntry.GetTransaction(), block))
		{
			filteredTransactions.push_back(txPoolEntry.GetTransaction());
			filteredEntriesByHash.insert(std::pair<Hash, TxPoolEntry>(txPoolEntry.GetTransaction().GetHash(), txPoolEntry));
		}
	}

	m_transactions.clear();

	const std::vector<Transaction> validTransactions = ValidTransactionFinder().FindValidTransactions(pBlockDB, pTxHashSet, filteredTransactions, pMemPoolAggTx);
	for (auto& transaction : validTransactions)
	{
		const TxPoolEntry& txPoolEntry = filteredEntriesByHash.at(transaction.GetHash());
		m_transactions.push_back(txPoolEntry);
	}
}

void Pool::ChangeStatus(const std::vector<Transaction>& transactions, const EDandelionStatus status)
{
	for (auto& txPoolEntry : m_transactions)
	{
		for (auto& transaction : transactions)
		{
			if (txPoolEntry.GetTransaction() == transaction)
			{
				txPoolEntry.SetStatus(status);
				break;
			}
		}
	}
}

bool Pool::ShouldEvict(const Transaction& transaction, const FullBlock& block) const
{
	const std::vector<TransactionInput>& blockInputs = block.GetTransactionBody().GetInputs();
	for (const TransactionInput& input : transaction.GetBody().GetInputs())
	{
		if (std::find(blockInputs.begin(), blockInputs.end(), input) != blockInputs.end())
		{
			return true;
		}
	}

	const std::vector<TransactionKernel>& blockKernels = block.GetTransactionBody().GetKernels();
	for (const TransactionKernel& kernel : transaction.GetBody().GetKernels())
	{
		if (std::find(blockKernels.begin(), blockKernels.end(), kernel) != blockKernels.end())
		{
			return true;
		}
	}

	return false;
}

std::unique_ptr<Transaction> Pool::Aggregate() const
{
	if (m_transactions.empty())
	{
		return std::unique_ptr<Transaction>(nullptr);
	}

	std::vector<Transaction> transactions;
	for (const TxPoolEntry& entry : m_transactions)
	{
		transactions.push_back(entry.GetTransaction());
	}

	return std::make_unique<Transaction>(TransactionAggregator::Aggregate(transactions));
}