#pragma once
#include <string>

const int& _get_bind_port();
const std::string& _get_server_root();
const int& _get_deploy_mode();

#define BIND_PORT _get_bind_port()
#define SERVER_ROOT _get_server_root()
// Deploy Mode: 0 Normal, 1 Rapid
#define DEPLOY_MODE _get_deploy_mode()