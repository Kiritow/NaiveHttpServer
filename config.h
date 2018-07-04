#pragma once
#include <string>

const int& _get_bind_port();
const std::string& _get_server_root();

#define BIND_PORT _get_bind_port()
#define SERVER_ROOT _get_server_root()
