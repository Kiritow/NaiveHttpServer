#pragma once
#include <string>
#include <map>

class Request
{
public:
	std::string method;
	std::string path;
	std::string http_version;
	std::map<std::string, std::string> header;
	// data contains data after http header. (work with POST requests)
	std::string data;
};

int parse_header(const std::string& header_raw, Request& req);
