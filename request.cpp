#include "request.h"
#include "config.h"
#include <vector>

using namespace std;

int parse_header(const std::string& header_raw,Request& req)
{
	int now = 0;
	size_t target;
	if (string::npos != (target = header_raw.find("\r\n")))
	{
		size_t endpos = 0;
		size_t beginpos = 0;
		if (string::npos != (endpos = header_raw.find(" ")) &&
			endpos < target)
		{
			req.method = header_raw.substr(0, endpos);
		}
		else return -1;
		beginpos = endpos + 1;
		if (string::npos != (endpos = header_raw.find(" ", beginpos)) &&
			endpos < target)
		{
			req.path = header_raw.substr(beginpos, endpos - beginpos);
		}
		else return -1;
		beginpos = endpos + 1;
		req.http_version = header_raw.substr(beginpos,target-beginpos);
		now = target + 2;
	}

	while (string::npos != (target = header_raw.find("\r\n", now)))
	{
		if (target - now == 0)
		{
			// This is the final \r\n\r\n, stop.
			break;
		}
		// Analyze this line [now,target)
		size_t endpos = 0;
		if (string::npos != (endpos = header_raw.find(":", now)) &&
			endpos < target)
		{
			size_t curpos = endpos + 1;
			while(header_raw[curpos] == ' ') curpos++;
			req.header[header_raw.substr(now, endpos-now)] = header_raw.substr(curpos, target - curpos);
		}
		else return -2;
		now = target + 2;
	}

	return 0;
}
