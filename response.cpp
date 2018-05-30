#include <ctime>

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
	case 416:
		header.append("416 Requested Range Not Satisfiable");
		setContent(default_header(header, "Invalid range request header."));
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

void Response::setContentRaw(const string& content)
{
	setContentLength(content.size());
	data = content;
}

void Response::setContent(const string & content, const string & content_type)
{
	setContentRaw(content);
	setContentType(content_type);
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

// Use struct tm::tm_wday for weekday value.
static const char* GetWeekAbbr(int weekday)
{
	static const char* w[7]{
		"Sun",
		"Mon","Tue","Wed","Thu","Fri",
		"Sat"
	};
	if (weekday < 0 || weekday>6) return "XXX";
	else return w[weekday];
}

static const char* GetMonthAbbr(int month)
{
	static const char* m[12]{
		"Jan","Feb","Mar","Apr","May",
		"Jun","Jul","Aug","Sep","Oct",
		"Nov","Dec"
	};
	if (month < 1 || month>12) return "XXX";
	else return m[month - 1];
}

// Fri, 09 Mar 2018 07:06:13 GMT
static string GetCurrentDateString()
{
	time_t t;
	struct tm* tt = NULL;
	time(&t);
	tt = localtime(&t);
	
	char buff[128] = { 0 };
	sprintf(buff, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		GetWeekAbbr(tt->tm_wday),
		tt->tm_mday, GetMonthAbbr(tt->tm_mon + 1), tt->tm_year + 1900,
		tt->tm_hour, tt->tm_min, tt->tm_sec);

	return buff;
}

int Response::send_with(sock & s)
{
	/// Server does not support keep-alive connection.
	set_raw("Connection", "close");
	set_raw("Server", "NaiveHTTPServer by Kiritow");
	set_raw("Date", GetCurrentDateString());

	string t = toString();
	
	logd("%s\n", t.c_str());

	return sendn(s, t);
}
