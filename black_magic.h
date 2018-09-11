#pragma once
#include "GSock/gsock.h"
#include <functional>

// Black Magic Entrance
int black_magic(serversock& t);

// Cross compile required
bool isHttpHeader(const std::string& header_raw);
int request_handler_main(sock& s, const std::string& header_raw);

// Shared control block between blocking and nonblocking IO.
struct conn_data
{
	string send_data;
	int send_done;

	string recv_data;

	// Callback Returns:
	// 0: Nothing.
	// 1: The socket will be removed from epoll and resouce will be released later.
	std::function<int(sock&, conn_data&)> onDataSent;
	std::function<int(sock&, conn_data&)> onNewData;

	// In Linux, binding is address of the epoll object.
	void* binding;
};

int request_handler_enter(sock& v, conn_data& data);
int response_handler_enter(sock& v, conn_data& data);