#include "HeaderAPI.h"
#include "../../JSONFactory.h"
#include "../NodeContext.h"

#include <Net/Util/HTTPUtil.h>
#include <BlockChain/BlockChainServer.h>
#include <Infrastructure/Logger.h>
#include <Common/Util/StringUtil.h>
#include <Common/Util/HexUtil.h>
#include <string>

//
// Handles requests to retrieve a single header by hash, height, or output commitment.
//
// APIs:
// GET /v1/headers/<hash>
// GET /v1/headers/<height>
// GET /v1/headers/<output commit>
//
int HeaderAPI::GetHeader_Handler(struct mg_connection* conn, void* pNodeContext)
{
	const std::string requestedHeader = HTTPUtil::GetURIParam(conn, "/v1/headers/");
	std::unique_ptr<BlockHeader> pBlockHeader = GetHeader(requestedHeader, ((NodeContext*)pNodeContext)->m_pBlockChainServer);

	if (nullptr != pBlockHeader)
	{
		const Json::Value headerNode = JSONFactory::BuildHeaderJSON(*pBlockHeader);
		return HTTPUtil::BuildSuccessResponse(conn, headerNode.toStyledString());
	}
	else
	{
		const std::string response = "HEADER NOT FOUND";
		return HTTPUtil::BuildBadRequestResponse(conn, response);
	}
}

std::unique_ptr<BlockHeader> HeaderAPI::GetHeader(const std::string& requestedHeader, IBlockChainServerPtr pBlockChainServer)
{
	if (requestedHeader.length() == 64 && HexUtil::IsValidHex(requestedHeader))
	{
		try
		{
			const Hash hash = Hash::FromHex(requestedHeader);
			std::unique_ptr<BlockHeader> pHeader = pBlockChainServer->GetBlockHeaderByHash(hash);
			if (pHeader != nullptr)
			{
				LOG_INFO_F("Found header with hash %s.", requestedHeader);
				return pHeader;
			}
			else
			{
				LOG_INFO_F("No header found with hash %s.", requestedHeader);
			}
		}
		catch (const std::exception&)
		{
			LOG_ERROR_F("Failed converting %s to a Hash.", requestedHeader);
		}
	}
	else if (requestedHeader.length() == 66 && HexUtil::IsValidHex(requestedHeader))
	{
		try
		{
			const Commitment outputCommitment(CBigInteger<33>::FromHex(requestedHeader));
			std::unique_ptr<BlockHeader> pHeader = pBlockChainServer->GetBlockHeaderByCommitment(outputCommitment);
			if (pHeader != nullptr)
			{
				LOG_INFO_F("Found header with output commitment %s.", requestedHeader);
				return pHeader;
			}
			else
			{
				LOG_INFO_F("No header found with commitment %s.", requestedHeader);
			}
		}
		catch (const std::exception&)
		{
			LOG_ERROR_F("Failed converting %s to a Commitment.", requestedHeader);
		}
	}
	else
	{
		try
		{
			std::string::size_type sz = 0;
			const uint64_t height = std::stoull(requestedHeader, &sz, 0);

			std::unique_ptr<BlockHeader> pHeader = pBlockChainServer->GetBlockHeaderByHeight(height, EChainType::CANDIDATE);
			if (pHeader != nullptr)
			{
				LOG_INFO_F("Found header at height %s.", requestedHeader);
				return pHeader;
			}
			else
			{
				LOG_INFO_F("No header found at height %s.", requestedHeader);
			}
		}
		catch (const std::invalid_argument&)
		{
			LOG_ERROR_F("Failed converting %s to height.", requestedHeader);
		}
	}

	return std::unique_ptr<BlockHeader>(nullptr);
}