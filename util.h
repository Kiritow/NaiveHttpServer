#pragma once
#include "GSock/gsock.h"
#include <string>
#include <map>

int sendn(sock& s, const std::string& in_data);

int recvline(sock& s, std::string& out);

bool endwith(const std::string& str, const std::string& target);

int urlencode(const std::string& url_before, std::string& out_url_encoded);

int urldecode(const std::string& url_before, std::string& out_url_decoded, std::map<std::string, std::string>& out_param);

int mymin(int a, int b);