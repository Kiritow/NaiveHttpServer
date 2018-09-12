#include "get.h"
#include "vmop.h"
#include "request.h"
#include "response.h"
#include "util.h"
#include "config.h"
#include "log.h"
#include "dirop.h"
#include <cstring>
using namespace std;

static int request_handler_get_dynamic(const Request& req,Response& res,
	const string& path_decoded, const map<string,string>& url_param)
{
	logd("Loading lua file: %s\n", path_decoded.c_str());

	string lua_code;
	int ret = GetFileContent(path_decoded, lua_code);
	if (ret < 0)
	{
		return -1;
	}

	VM v;
	auto L = v.get();
	lua_newtable(L);
	lua_pushstring(L, req.http_version.c_str());
	lua_setfield(L, 1, "http_version"); // request["http_version"]=...

	lua_pushstring(L, "GET");
	lua_setfield(L, 1, "method"); // request["method"]="GET"

	for (const auto& pr : req.header)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 1, pr.first.c_str()); // request[...]=...
	}

	// Parameter table
	lua_newtable(L);
	for (const auto& pr : url_param)
	{
		lua_pushstring(L, pr.second.c_str());
		lua_setfield(L, 2, pr.first.c_str()); // request["param"][...]=...
	}
	lua_setfield(L, 1, "param");

	lua_setglobal(L, "request");

	// Lua CGI program should fill the response table.
	// response.output will be outputed as content.
	lua_newtable(L);
	lua_setglobal(L, "response");

	logd("Preparing helper...\n");

	if (v.runCode("helper={} helper.print=function(...) local t=table.pack(...) "s +
		"if(response.output==nil) then response.output='' end " +
		"local temp='' " +
		"for i,v in ipairs(t) do " +
		"if(#(temp)>0) then temp = temp .. '\\t' end " +
		"temp = temp .. tostring(v) " +
		"end " +
		"response.output = response.output .. temp .. '\\n' " +
		"end ") < 0)
	{
		loge("Failed to prepare helper.\n");
		return -2;
	}

	logd("Executing lua file: %s\n", path_decoded.c_str());
	if (v.runCode(lua_code) < 0)
	{
		loge("Failed to run user lua code.\n");
		return -3;
	}

	logd("Execution finished successfully.\n");

	v.getglobal("response");

	if (!lua_istable(L, -1)) // type(response)~="table"
	{
		logd("LuaVM: variable 'response' is not a table.\n");
		return -4;
	}

	v.pushnil();

	while (lua_next(L, -2))
	{
		if (lua_isstring(L, -2))  // type(key)=="string"
		{
			const char* item_name = lua_tostring(L, -2);
			const char* item_value = lua_tostring(L, -1);

			if ((!item_name) || (!item_value))
			{
				logd("LuaVM: An item cannot be converted to string. Key: %s\n", item_name);
			}
			else
			{
				if (strcmp(item_name, "output") != 0)
				{
					res.set_raw(item_name, item_value);
				}
				else
				{
					res.setContentRaw(item_value);
				}
			}
		}
		lua_pop(L, 1);
	}

	res.set_code(200);
	return 0;
}

int request_handler_get(const Request& req, Response& res)
{
	// URL decoded path
	string path;
	map<string, string> url_param;
	if (urldecode(req.path, path, url_param) < 0)
	{
		loge("Failed to decode url : %s\n", req.path.c_str());
		return -1;
	}

	// Request to / would be dispatched to /index.html or /index.lua
	if (endwith(path, "/"))
	{
		Request xreq = req;
		xreq.path += "index.html";
		if (request_handler_get(xreq, res) > 0)
		{
			Request yreq = req;
			yreq.path += "index.lua";
			if (request_handler_get(yreq, res) > 0)
			{
				// Display a list
				string ans;

				ans.append("<html><head><title>Index of " + path + "</title></head><body><h1>Index of " + path + "</h1><ul>");
				string real_dir = SERVER_ROOT + path;
				logd("About to list Directory: %s\n", real_dir.c_str());
				DirWalk w(real_dir);
				logd("DirWalk Created.\n");
				string filename;
				int is_dir;
				while (w.next(filename, is_dir) > 0)
				{
					string realname;
					urlencode(filename, realname);

					ans.append("<li><a href='" + realname);
					if (is_dir)
					{
						ans.append("/");
					}
					ans.append("'>" + filename);
					if (is_dir)
					{
						ans.append("/");
					}
					ans.append("</a></li>");
				}
				ans.append("</ul></body></html>");

				res.set_code(200);
				res.setContent(ans);
			}
		}
		return 0;
	}

	int request_type = get_request_path_type(path);
	if (request_type < 0)
	{
		// Invalid request (File not found)
		res.set_code(404);
		return 1;
	}
	else if (request_type == 0)
	{
		// Static Target
		// Just read out and send it.
		int content_length;
		if (GetFileLength(path, content_length) < 0)
		{
			// File not readable.
			res.set_code(500);
			return 0;
		}

		// Requesting partial content?
		auto range_iter = req.header.find("Range");
		if (range_iter != req.header.end())
		{
			int beginat, length;
			// FIXME: Why mp["Range"] cause compile error?
			if (parse_range_request(range_iter->second, content_length, beginat, length) < 0)
			{
				res.set_code(416);
				return 0;
			}

			logd("Range Request: begin: %d, length: %d\n", beginat, length);
			string content_type;
			if (GetFileContentType(path, content_type) < 0) content_type = "text/plain";

			if (length != content_length)
			{
				// partial content
				string content;
				if (GetFileContentEx(path, beginat, length, content) < 0)
				{
					/// Error while reading file.
					res.set_code(500);
					return 0;
				}
				
				res.set_code(206);
				res.setContent(content, content_type);
			}
			else
			{
				// full content
				string content;
				if (GetFileContent(path, content) < 0)
				{
					/// Error while reading file.
					res.set_code(500);
					return 0;
				}
				res.set_code(200);
				res.setContent(content, content_type);
			}

			char content_range_buff[64] = { 0 };
			sprintf(content_range_buff, "bytes %d-%d/%d", beginat, beginat + length - 1, content_length);
			res.set_raw("Content-Range", content_range_buff);

			res.set_raw("Accept-Ranges", "bytes");
			return 0;
		}
		else
		{
			// Just a normal request without Range in request header.
			string content_type;
			if (GetFileContentType(path, content_type) < 0) content_type = "text/plain";

			string content;
			if (GetFileContent(path, content) < 0)
			{
				/// Error while reading file.
				res.set_code(500);
				return 0;
			}
			res.set_code(200);
			res.set_raw("Accept-Ranges", "bytes");
			res.setContent(content, content_type);

			return 0;
		}
	}
	else
	{
		// Dynamic Target
		if (request_handler_get_dynamic(req, res, path, url_param) < 0)
		{
			res.set_code(500);
		}
		return 0;
	}
}