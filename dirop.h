#pragma once
#include <string>

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
