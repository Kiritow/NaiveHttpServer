#pragma once
#include <string>
#include <map>

int parse_header(const std::string& header_raw,
	std::string& method,
	std::string& path,
	std::string& http_version,
	std::map<std::string, std::string>& outmap);

int get_request_type(const std::string& path);