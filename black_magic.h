#pragma once
#include "GSock/gsock.h"

// Black Magic Entrance
int black_magic(serversock& t);

// Cross compile required
bool isHttpHeader(const std::string& header_raw);
int request_handler_main(sock& s, const std::string& header_raw);