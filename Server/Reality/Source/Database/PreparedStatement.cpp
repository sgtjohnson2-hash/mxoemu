#include <winsock2.h>
#include "PreparedStatement.h"
#include "Database.h"
#include "../Log.h"
#include <string>

PreparedStatement::PreparedStatement(const std::string& query)
	: m_query(query)
{
}

PreparedStatement::~PreparedStatement()
{
}

void PreparedStatement::SetUInt16(uint32_t index, uint16_t value)
{
	m_params[index] = std::to_string(value);
}

void PreparedStatement::SetInt32(uint32_t index, int32_t value)
{
	m_params[index] = std::to_string(value);
}

void PreparedStatement::SetUInt32(uint32_t index, uint32_t value)
{
	m_params[index] = std::to_string(value);
}

void PreparedStatement::SetUInt64(uint32_t index, uint64_t value)
{
	m_params[index] = std::to_string(value);
}

void PreparedStatement::SetDouble(uint32_t index, double value)
{
	m_params[index] = std::to_string(value);
}

void PreparedStatement::SetString(uint32_t index, const std::string& value)
{
	m_params[index] = "'" + value + "'"; // We'll escape it during GetQueryString
}

std::string PreparedStatement::GetQueryString(Database* db) const
{
	std::string finalQuery = m_query;
	for (auto it = m_params.rbegin(); it != m_params.rend(); ++it)
	{
		std::string marker = "?" + std::to_string(it->first);
		size_t pos = finalQuery.find(marker);
		if (pos != std::string::npos)
		{
			std::string val = it->second;
			if (val.length() >= 2 && val.front() == '\'' && val.back() == '\'')
			{
				std::string inner = val.substr(1, val.length() - 2);
				val = "'" + db->EscapeString(inner) + "'";
			}
			finalQuery.replace(pos, marker.length(), val);
		}
	}
	return finalQuery;
}
