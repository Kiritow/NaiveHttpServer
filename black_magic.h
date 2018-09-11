#pragma once
#include "GSock/gsock.h"
#include "request.h"
#include "response.h"

// Black Magic Entrance
int black_magic(serversock& t);

// Cross compile required
int request_handler(const Request& req, Response& res);