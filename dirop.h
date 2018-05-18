#pragma once
#include <string>
#ifdef _WIN32
#include <io.h> // access
#else
#include <unistd.h>
#endif

class DirWalk
{
public:
	DirWalk();
	DirWalk(const std::string& dirname);
	~DirWalk();

	void walk(const std::string& dirname);
	int next(std::string& filename,int& is_dir);
private:
    struct _impl;
    _impl* _p;
};
