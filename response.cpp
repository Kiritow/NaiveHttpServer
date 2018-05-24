#include "response.h"
#include "util.h"
#include "log.h"
using namespace std;

static string default_header(const string& header, const string& info)
{
	return string("<html><head><title>") + header + "</title></head><body><h1>" + header + "</h1>" + info + "</body></html>";
}

/// Set code will reset response status
void Response::set_code(int code)
{
	header = "HTTP/1.1 ";
	switch (code)
	{
	case 200:
		header.append("200 OK");
		break;
	case 400:
		header.append("400 Bad Request");
		break;
	case 403:
		header.append("403 Forbidden");
		break;
	case 404:
		header.append("404 Not Found");
		setContent("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1>The request is not found on this server.</body></html>");
		break;
	case 405:
		header.append("405 Method Not Allowed");
		setContent(default_header(header, "The method is not allowed."));
		break;
	case 500:
		header.append("500 Internal Server Error");
		setContent(default_header(header, "Server has encoutered an internal error while processing your request."));
		break;
	case 501:
		header.append("501 Not Implemented");
		setContent(default_header(header, "The method is not implemented."));
		break;
	case 503:
		header.append("503 Service Unavailable");
		setContent(default_header(header, "Service is not available for now. Please try later."));
		break;
	}
	header.append("\r\n");
}

void Response::set_raw(const string & name, const string & value)
{
	mp[name] = value;
}

void Response::setContentLength(int length)
{
	set_raw("Content-Length", to_string(length));
}

void Response::setContentType(const string & content_type)
{
	set_raw("Content-Type", content_type);
}

void Response::setContent(const string & content, const string & content_type)
{
	setContentLength(content.size());
	setContentType(content_type);
	data = content;
}

string Response::toString()
{
	string ans;
	ans.append(header);
	for (auto& pr : mp)
	{
		ans.append(pr.first + ": " + pr.second + "\r\n");
	}
	ans.append("\r\n");
	if (!data.empty())
	{
		ans.append(data);
	}
	return ans;
}

int Response::send_with(sock & s)
{
	/// Server does not support keep-alive connection.
	set_raw("Connection", "close");
	string t = toString();
	logd("%s\n", t.c_str());
	return sendn(s, t);
}
