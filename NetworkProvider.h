#pragma once
#include <string>
#include "GSock/gsock.h"

class NetworkProvider
{
public:
	NetworkProvider();
	void send(const void* data, int size);
	void send(const std::string& data);
	
	std::string& getdata();
	void clear();

	// This value is used by user and will never be changed by NetworkProvider.
	void* user_data;

	// The following value should not be used and set.
	std::string _tosend;
	std::string _data;
};
