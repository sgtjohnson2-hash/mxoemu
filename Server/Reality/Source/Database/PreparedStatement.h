#ifndef MXOSIM_PREPAREDSTATEMENT_H
#define MXOSIM_PREPAREDSTATEMENT_H

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

class PreparedStatement
{
public:
	PreparedStatement(const std::string& query);
	~PreparedStatement();

	void SetUInt16(uint32_t index, uint16_t value);
	void SetInt32(uint32_t index, int32_t value);
	void SetUInt32(uint32_t index, uint32_t value);
	void SetUInt64(uint32_t index, uint64_t value);
	void SetDouble(uint32_t index, double value);
	void SetString(uint32_t index, const std::string& value);

	std::string GetQueryString(class Database* db) const;

private:
	std::string m_query;
	std::map<uint32_t, std::string> m_params;
};

#endif
