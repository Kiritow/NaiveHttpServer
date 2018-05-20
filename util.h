#pragma once
#include "GSock/gsock.h"
#include <string>

int sendn(sock& s, const std::string& in_data);

int recvline(sock& s, std::string& out);

bool endwith(const std::string& str, const std::string& target);
