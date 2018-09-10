#include "parser.h"
#include "util.h"
#include <cstring>
using namespace std;

int parse_range_request(const string& range, int content_length, int& _out_beginat, int& _out_length)
{
	const char* s = range.c_str();
	const char* t = s + range.size();
	const char* p = strstr(s, "bytes=");
	if (p == NULL)
	{
		return -1;
	}
	// bytes=...
	p = p + 6;
	const char* q = strstr(p, "-");
	if (q == NULL)
	{
		return -2;
	}

	int beginat;
	if (q - p > 0)
	{
		// bytes=A-...
		sscanf(p, "%d", &beginat);
	}
	else
	{
		// bytes=-...
		beginat = -1;
	}

	q++;
	int endat;
	if (t - q > 0)
	{
		// bytes=?-B
		sscanf(q, "%d", &endat);
	}
	else
	{
		// bytes=?-
		endat = -1;
	}

	if (beginat < 0 && endat < 0)
	{
		return -3;
	}

	if (beginat < 0)
	{
		// bytes=-B
		_out_length = endat;
		_out_beginat = content_length - _out_length;
	}
	else if (endat < 0)
	{
		// bytes=A-
		_out_beginat = beginat;
		_out_length = content_length - beginat;
	}
	else
	{
		// bytes=A-B
		_out_beginat = beginat;
		_out_length = mymin(content_length - beginat, endat - beginat + 1);
	}

	return 0;
}
