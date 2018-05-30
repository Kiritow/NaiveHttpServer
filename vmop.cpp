#include "vmop.h"
#include "log.h"
using namespace std;

VM::VM()
{
	_luavm = luaL_newstate();
	luaL_openlibs(_luavm);
	runCode("response={}");
}


VM::~VM()
{
	lua_close(_luavm);
}

lua_State* VM::get()
{
	return _luavm;
}

int VM::runCode(const string& LuaSource)
{
	bool err = luaL_loadbuffer(_luavm, LuaSource.c_str(), (int)LuaSource.size(), "LuaVM") || lua_pcall(_luavm, 0, 0, 0);
	if (err) {
		loge("LuaVM Error: %s\n", lua_tostring(_luavm, -1));
		lua_pop(_luavm, 1);
		return -1;
	}
	return 0;
}

int VM::getglobal(const char* name)
{
	return lua_getglobal(_luavm, name);
}

void VM::pushnil()
{
	lua_pushnil(_luavm);
}