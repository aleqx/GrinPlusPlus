#pragma once

// Copyright (c) 2018-2019 David Burkett
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#include <Crypto/BigInteger.h>
#include <Common/Util/BitUtil.h>
#include <Common/Util/HexUtil.h>
#include <Core/Traits/Printable.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>

class Commitment : public Traits::IPrintable
{
public:
	//
	// Constructors
	//
	Commitment(CBigInteger<33>&& commitmentBytes)
		: m_commitmentBytes(std::move(commitmentBytes))
	{

	}
	Commitment(const CBigInteger<33>& commitmentBytes)
		: m_commitmentBytes(commitmentBytes)
	{

	}
	Commitment(const Commitment& other) = default;
	Commitment(Commitment&& other) noexcept = default;

	//
	// Destructor
	//
	~Commitment() = default;

	//
	// Operators
	//
	Commitment& operator=(const Commitment& other) = default;
	Commitment& operator=(Commitment&& other) noexcept = default;
	inline bool operator<(const Commitment& rhs) const { return m_commitmentBytes < rhs.GetBytes(); }
	inline bool operator!=(const Commitment& rhs) const { return m_commitmentBytes != rhs.GetBytes(); }
	inline bool operator==(const Commitment& rhs) const { return m_commitmentBytes == rhs.GetBytes(); }

	//
	// Getters
	//
	inline const CBigInteger<33>& GetBytes() const { return m_commitmentBytes; }

	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const
	{
		serializer.AppendBigInteger<33>(m_commitmentBytes);
	}

	static Commitment Deserialize(ByteBuffer& byteBuffer)
	{
		return Commitment(byteBuffer.ReadBigInteger<33>());
	}

	std::string ToHex() const
	{
		return HexUtil::ConvertToHex(m_commitmentBytes.GetData());
	}

	//
	// Traits
	//
	virtual std::string Format() const override final { return m_commitmentBytes.Format(); }

private:
	// The 33 byte commitment.
	CBigInteger<33> m_commitmentBytes;
};

namespace std
{
	template<>
	struct hash<Commitment>
	{
		size_t operator()(const Commitment& commitment) const
		{
			const std::vector<unsigned char>& bytes = commitment.GetBytes().GetData();
			return BitUtil::ConvertToU64(bytes[0], bytes[4], bytes[8], bytes[12], bytes[16], bytes[20], bytes[24], bytes[28]);
		}
	};
}