#include "util.h"
#include "log.h"
#include "config.h"
#include "GSock/gsock_helper.h"
#include <cstring>
using namespace std;

#ifdef _WIN32
#include <io.h> // access
#else
#include <unistd.h>
#endif

bool endwith(const string& str, const string& target)
{
	size_t ans = str.rfind(target);
	return ans == str.size() - target.size();
}

static inline bool urlCharNeedEncode(char c)
{
	if ((c >= 'A'&&c <= 'Z') || (c >= 'a'&&c <= 'z') || (c >= '0'&&c <= '9') ||
		(c == '-') || (c == '_') || (c == '.') || (c == '!') ||
		(c == '~') || (c == '*') || (c == '\'') || (c == '(') ||
		(c == ',') || (c == ')'))
	{
		return false;
	}
	else
	{
		return true;
	}
}

static inline int getHexValue(char c)
{
	if (c >= '0'&&c <= '9')
	{
		return c - '0';
	}
	else if (c >= 'a'&&c <= 'f')
	{
		return c - 'a' + 10;
	}
	else if (c >= 'A'&&c <= 'F')
	{
		return c - 'A' + 10;
	}
	else
	{
		logw("[getHexValue]: Failed to get hex value from char: %c\n", c);
		return -1;
	}
}

int urlencode(const string& url_before, string& out_url_encoded)
{
	string ans;
	for (const auto& c : url_before)
	{
		if (urlCharNeedEncode(c))
		{
			char buff[8] = { 0 };
			sprintf(buff, "%%%02X", (unsigned char)c);
			ans.append(buff);
		}
		else
		{
			ans.push_back(c);
		}
	}
	out_url_encoded = ans;
	return 0;
}

int urldecode_real(const string& url_before, string& out_url_decoded)
{
	string ans;
	int len = url_before.size();
	for (int i = 0; i < len; i++)
	{
		if (url_before[i] == '%')
		{
			int a = getHexValue(url_before[i + 1]);
			int b = getHexValue(url_before[i + 2]);
			char c = a * 16 + b;
			ans.push_back(c);
			i += 2;
		}
		else
		{
			ans.push_back(url_before[i]);
		}
	}
	out_url_decoded = ans;

	return 0;
}

int urldecode(const string& url_before, string& out_url_decoded, map<string, string>& out_param)
{
	size_t idx = url_before.find("?");
	if (idx == string::npos)
	{
		// No parameters. Fallback to urldecode.
		return urldecode_real(url_before, out_url_decoded);
	}
	else // ....?para=value&para2=value
	{
		// okay we have params, yeah?
		out_param.clear();

		string frontpart = url_before.substr(0, idx);
		urldecode_real(frontpart, out_url_decoded);
		int len = url_before.size();
		int now = idx + 1;
		// Things get weird...
		while (true)
		{
			size_t nidx = url_before.find("&", now);
			size_t endpoint;
			if (nidx != string::npos)
			{
				// Still have next part
				endpoint = nidx;
			}
			else
			{
				// Oh we are at the end
				endpoint = len;
			}

			// separate it
			string current = url_before.substr(now, endpoint - now);
			size_t midx = current.find("=");
			if (midx != string::npos)
			{
				// param=
				if (midx == current.size() - 1)
				{
					string a = current.substr(0, midx);
					string ta;
					urldecode_real(a, ta);
					out_param.insert(make_pair(ta, ""));
				}
				// param=value
				else
				{
					string a = current.substr(0, midx);
					string ta;
					urldecode_real(a, ta);
					string b = current.substr(midx + 1);
					string tb;
					urldecode_real(b, tb);
					out_param.insert(make_pair(ta, tb));
				}
			}

			// This is the end.
			if (nidx == string::npos) break;
			now = endpoint + 1;
		}

		return 0;
	}
}

int mymin(int a, int b)
{
	return a < b ? a : b;
}

int GetFileContent(const string& request_path, string& out_content)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	char buff[1024];
	string content;
	while (true)
	{
		memset(buff, 0, 1024);
		int ret = fread(buff, 1, 1024, fp);
		if (ret <= 0)
		{
			break;
		}
		content.append(string(buff, ret));
	}
	fclose(fp);
	out_content = content;
	return 0;
}

int GetFileContentEx(const string& request_path, int beginat, int length, string& out_content)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	fseek(fp, beginat, SEEK_SET);
	char buff[1024];
	string content;
	int done = 0;
	int total = length;
	while (done < total)
	{
		memset(buff, 0, 1024);
		int ret = fread(buff, 1, mymin(1024, total - done), fp);
		if (ret <= 0)
		{
			break;
		}
		content.append(string(buff, ret));
		done += ret;
	}
	fclose(fp);
	out_content = content;
	return 0;
}

int GetFileLength(const string& request_path, int& out_length)
{
	string realpath = SERVER_ROOT + request_path;
	FILE* fp = fopen(realpath.c_str(), "rb");
	if (fp == NULL) return -1;
	fseek(fp, 0L, SEEK_END);
	out_length = ftell(fp);
	fclose(fp);
	return 0;
}

#define ct(abbr,target) else if(endwith(path,abbr)) out_content_type=target

int GetFileContentType(const string& path, string& out_content_type)
{
	if (endwith(path, ".html"))
	{
		out_content_type = "text/html";
	}
	ct(".bmp", "application/x-bmp");
	ct(".doc", "application/msword");
	ct(".ico", "image/x-icon");
	ct(".java", "java/*");
	ct(".class", "java/*");
	ct(".jpeg", "image/jpeg");
	ct(".jpg", "image/jpeg");
	ct(".png", "image/png");
	ct(".swf", "application/x-shockwave-flash");
	ct(".xhtml", "text/html");
	ct(".apk", "application/vnd.android.package-archive");
	ct(".exe", "application/x-msdownload");
	ct(".htm", "text/html");
	ct(".js", "application/x-javascript");
	ct(".mp3", "audio/mp3");
	ct(".mp4", "video/mpeg4");
	ct(".mpg", "video/mpg");
	ct(".pdf", "application/pdf");
	ct(".rmvb", "application/vnd.rn-realmedia-vbr");
	ct(".torrent", "application/x-bittorrent");
	ct(".txt", "text/plain");
	ct(".css", "text/css");
	else
	{
		/// Not Support Content Type
		return -1;
	}

	/// Supported ContentType
	return 0;
}

#undef ct

int get_request_path_type(const string& path)
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
		if (endwith(path,".lua")) // XXX.lua
		{
			// Dynamic Request
			return 1;
		}
		else
		{
			// Static Request
			// Notice: Binary file should be dynamic request (like CGI), but here just deal it as static file.
			return 0;
		}
	}
}

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