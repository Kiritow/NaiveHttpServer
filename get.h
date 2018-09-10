#include "GSock/gsock.h"
#include <string>
#include <map>

int request_get_handler(sock& s, const std::string& in_path, const std::string& version, const std::map<std::string, std::string>& mp);