#include "util.h"
using namespace std;

int sendn(sock& s, const std::string& in_data)
{
	int done = 0;
	int total = in_data.size();
	while (done < total)
	{
		int ret = s.send(in_data.c_str() + done, total - done);
		if (ret <= 0)
		{
			return ret;
		}
		else
		{
			done += ret;
		}
	}
	return done;
}

int recvline(sock& s, std::string& out)
{
	std::string ans;
	char buff[16];
	while (true)
	{
		buff[0] = buff[1] = 0;
		int ret = s.recv(buff, 1);
		if (ret<0) return ret;
		else if (ret == 0)
			return -2; /// If connection is closed... (BUG)
		ans.push_back(buff[0]);
		if (ans.find("\r\n") != std::string::npos)
		{
			break;
		}
	}
	out = ans;

	return ans.size();
}

bool endwith(const string& str, const string& target)
{
	size_t ans = str.rfind(target);
	return ans == str.size() - target.size();
}