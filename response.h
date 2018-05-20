#pragma once
#include "GSock/gsock.h"
#include <string>
#include <map>

class Response
{
public:
	/// Set code will reset response status
	void set_code(int code);

	void set_raw(const std::string& name, const std::string& value);

	void setContentLength(int length);

	void setContentType(const std::string& content_type);

	void setContent(const std::string& content, const std::string& content_type = "text/html");

	std::string toString();

	int send_with(sock& s);
private:
	std::string header;
	std::map<std::string, std::string> mp;
	std::string data;
};