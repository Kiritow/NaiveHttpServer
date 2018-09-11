#pragma once
#include <string>
#include <map>

class Request
{
public:
	Request();
	// This method should be called first to check if this Request is valid.
	bool isReady();

	std::string method;
	std::string path;
	std::string http_version;
	std::map<std::string, std::string> param;
	// -1 Invalid
	// 0 Static
	// 1 Dynamic
	int request_type;
private:
	bool _ready;

	friend Request parse_header(const std::string&);
};

// This function assume that header_raw contains a completed header.
Request parse_header(const std::string& header_raw);

int get_request_type(const string& path);