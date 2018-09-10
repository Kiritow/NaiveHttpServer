#pragma once
#include "GSock/gsock.h"
#include "response.h"
#include <string>
#include <map>

int sendn(sock& s, const std::string& in_data);

int recvline(sock& s, std::string& out);

bool endwith(const std::string& str, const std::string& target);

int urlencode(const std::string& url_before, std::string& out_url_encoded);

int urldecode(const std::string& url_before, std::string& out_url_decoded, std::map<std::string, std::string>& out_param);

int mymin(int a, int b);

int GetFileContent(const std::string& request_path, std::string& out_content);

int GetFileContentEx(const std::string& request_path, int beginat, int length, std::string& out_content);

int GetFileLength(const std::string& request_path, int& out_length);

int GetFileContentType(const std::string& path, std::string& out_content_type);

int ReadFileAndSend(Response& req, sock& s, const std::string& request_path);
