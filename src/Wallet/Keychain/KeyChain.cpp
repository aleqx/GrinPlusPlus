#include "KeyChain.h"
#include "SeedEncrypter.h"
#include "KeyGenerator.h"

#include <Wallet/Exceptions/KeyChainException.h>
#include <Common/Exceptions/UnimplementedException.h>
#include <Common/Util/VectorUtil.h>

KeyChain::KeyChain(const Config& config, PrivateExtKey&& masterKey, SecretKey&& bulletProofNonce)
	: m_config(config), m_masterKey(std::move(masterKey)), m_bulletProofNonce(std::move(bulletProofNonce))
{

}

KeyChain KeyChain::FromSeed(const Config& config, const SecureVector& masterSeed)
{
	PrivateExtKey masterKey = KeyGenerator(config).GenerateMasterKey(masterSeed, EKeyChainType::DEFAULT);
	SecretKey bulletProofNonce = Crypto::BlindSwitch(masterKey.GetPrivateKey(), 0);
	return KeyChain(config, std::move(masterKey), std::move(bulletProofNonce));
}

KeyChain KeyChain::ForGrinbox(const Config& config, const SecureVector& masterSeed)
{
	PrivateExtKey masterKey = KeyGenerator(config).GenerateMasterKey(masterSeed, EKeyChainType::DEFAULT);
	SecretKey rootKey = Crypto::BlindSwitch(masterKey.GetPrivateKey(), 713);
	masterKey = KeyGenerator(config).GenerateMasterKey(SecureVector(rootKey.data(), rootKey.data() + rootKey.size()), EKeyChainType::GRINBOX);
	SecretKey bulletProofNonce = Crypto::BlindSwitch(masterKey.GetPrivateKey(), 0);
	return KeyChain(config, std::move(masterKey), std::move(bulletProofNonce));
}

SecretKey KeyChain::DerivePrivateKey(const KeyChainPath& keyPath) const
{
	KeyGenerator keygen(m_config);

	PrivateExtKey privateKey(m_masterKey);
	for (const uint32_t childIndex : keyPath.GetKeyIndices())
	{
		privateKey = keygen.GenerateChildPrivateKey(privateKey, childIndex);
	}

	return privateKey.GetPrivateKey();
}

SecretKey KeyChain::DerivePrivateKey(const KeyChainPath& keyPath, const uint64_t amount) const
{
	return Crypto::BlindSwitch(DerivePrivateKey(keyPath), amount);
}

std::unique_ptr<RewoundProof> KeyChain::RewindRangeProof(const Commitment& commitment, const RangeProof& rangeProof, const EBulletproofType& bulletproofType) const
{
	if (bulletproofType == EBulletproofType::ORIGINAL)
	{
		const SecretKey nonce = CreateNonce(commitment, m_bulletProofNonce);
		return Crypto::RewindRangeProof(commitment, rangeProof, nonce);
	}
	else if (bulletproofType == EBulletproofType::ENHANCED)
	{
		PublicKey masterPublicKey = Crypto::CalculatePublicKey(m_masterKey.GetPrivateKey());

		const SecretKey rewindNonceHash = Crypto::Blake2b(masterPublicKey.GetCompressedBytes().GetData());

		return Crypto::RewindRangeProof(commitment, rangeProof, CreateNonce(commitment, rewindNonceHash));
	}

	throw UNIMPLEMENTED_EXCEPTION;
}

RangeProof KeyChain::GenerateRangeProof(
	const KeyChainPath& keyChainPath, 
	const uint64_t amount, 
	const Commitment& commitment, 
	const SecretKey& blindingFactor, 
	const EBulletproofType& bulletproofType) const
{
	const ProofMessage proofMessage = ProofMessage::FromKeyIndices(keyChainPath.GetKeyIndices(), bulletproofType);

	if (bulletproofType == EBulletproofType::ORIGINAL)
	{
		const SecretKey nonce = CreateNonce(commitment, m_bulletProofNonce);

		return Crypto::GenerateRangeProof(amount, blindingFactor, nonce, nonce, proofMessage);
	}
	else if (bulletproofType == EBulletproofType::ENHANCED)
	{
		const SecretKey privateNonceHash = Crypto::Blake2b(m_masterKey.GetPrivateKey().GetBytes().GetData());

		PublicKey masterPublicKey = Crypto::CalculatePublicKey(m_masterKey.GetPrivateKey());
		const SecretKey rewindNonceHash = Crypto::Blake2b(masterPublicKey.GetCompressedBytes().GetData());

		return Crypto::GenerateRangeProof(amount, blindingFactor, CreateNonce(commitment, privateNonceHash), CreateNonce(commitment, rewindNonceHash), proofMessage);
	}
	
	throw UNIMPLEMENTED_EXCEPTION;
}

SecretKey KeyChain::CreateNonce(const Commitment& commitment, const SecretKey& nonceHash) const
{
	return Crypto::Blake2b(commitment.GetBytes().GetData(), nonceHash.GetBytes().GetData());
}