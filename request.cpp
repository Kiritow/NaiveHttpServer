#include "request.h"
#include "config.h"
#include <vector>

#ifdef _WIN32
#include <io.h> // access
#else
#include <unistd.h>
#endif

using namespace std;

// Notice: \r\n at end of each line is removed.
static int split_header_raw(const string& header_raw, vector<string>& outvec)
{
	int now = 0;
	size_t target;
	vector<string> vec;
	while (string::npos != (target = header_raw.find("\r\n", now)))
	{
		if (target - now == 0)
		{
			// This is the final \r\n\r\n, stop.
			break;
		}
		vec.push_back(header_raw.substr(now, target - now));
		now = target + 2;
	}
	outvec = move(vec); // move would be faster?
	return 0;
}

static int parse_header_firstline(const string& http_firstline, string& method, string& path, string& http_version)
{
	size_t target = 0;
	if (string::npos == (target = http_firstline.find(" ")))
	{
		return -1;
	}
	method = http_firstline.substr(0, target);
	target++;
	if (string::npos == (target = http_firstline.find(" ", target)))
	{
		return -2;
	}
	path = http_firstline.substr(method.size() + 1, target - method.size() - 1);
	http_version = http_firstline.substr(target + 1);
	return 0;
}

static int parse_header_rawline(const string& header_rawline, pair<string, string>& outpr)
{
	size_t target;
	if (string::npos == (target = header_rawline.find(":")))
	{
		return -1;
	}
	else
	{
		outpr.first = header_rawline.substr(0, target);
		// Skip space
		target++;
		while (header_rawline[target] == ' ') target++;
		outpr.second = header_rawline.substr(target);
		return 0;
	}
}

int parse_header(const string& header_raw, string& method, string& path, string& http_version, map<string, string>& outmap)
{
	vector<string> header_vec;
	int ret = split_header_raw(header_raw, header_vec);
	if (ret < 0)
	{
		return -1;
	}

	if (parse_header_firstline(header_vec[0], method, path, http_version) < 0)
	{
		return -2;
	}

	map<string, string> header_map;
	int sz = header_vec.size();
	for (int i = 1; i < sz; i++)
	{
		pair<string, string> pr;
		if (parse_header_rawline(header_vec[i], pr) < 0)
		{
			return -3;
		}
		else
		{
			header_map.insert(pr);
		}
	}
	outmap = move(header_map);
	return 0;
}

int get_request_type(const string& path)
{
	string realpath = SERVER_ROOT + path;
	if (access(realpath.c_str(), 0) < 0) // File not exist
	{
		// File not exist, maybe dynamic request?
		// Only Lua extension is planned to support, which means *.php will be treated as a static file.
		realpath = realpath + ".lua";
		if (access(realpath.c_str(), 0) < 0)
		{
			// Not a valid request.
			return -1;
		}
		else
		{
			/// Dynamic Request
			return 1;
		}
	}
	else // File exists.
	{
		if (path.find(".lua") == path.size() - 4) // XXX.lua
		{
			/// Dynamic Request
			return 1;
		}
		else
		{
			/// Static Request
			/// Notice: Binary file should be dynamic request (like CGI), but here just deal it as static file.
			return 0;
		}
	}
}