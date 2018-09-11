#include "NetworkProvider.h"
using namespace std;

NetworkProvider::NetworkProvider()
{
	user_data = nullptr;
}

void NetworkProvider::send(const std::string& data)
{
	_tosend.append(data);
}

void NetworkProvider::send(const void* data, int size)
{
	_tosend.append(string((char*)data, size));
}

std::string& NetworkProvider::getdata()
{
	return _data;
}

void NetworkProvider::clear()
{
	_data.clear();
}