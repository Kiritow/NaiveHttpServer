#pragma once
#include <string>
#include <functional>
#include "LuaSrc/include/lua.hpp"

class VM
{
public:
	VM();
	/// NonMoveable,NonCopyable
	VM(const VM&) = delete;
	VM& operator = (const VM&) = delete;
	VM(VM&&) = delete;
	VM& operator = (VM&&) = delete;
	~VM();

	int runCode(const std::string& LuaSource);

	// Lua C API Wrapper
	int getglobal(const char* name); // Lua -> Stack
	void pushnil(); // C -> Stack

	// ONLY For Experimental Use.
	// This method should be removed in future.
	lua_State* get();
private:
	lua_State * _luavm;
};
