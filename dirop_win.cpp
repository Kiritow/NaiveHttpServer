#ifdef _WIN32
#include "dirop.h"
#include "io.h"
using namespace std;

struct DirWalk::_impl
{
	int _status;
	string _dirname;
	int _handle;
};

DirWalk::DirWalk() : _p(new _impl)
{
	_p->_status = 0;
}

DirWalk::DirWalk(const string& dirname) : DirWalk()
{
	walk(dirname);
}

DirWalk::~DirWalk()
{
	walk(""); /// Just call walk to clean up resource
}

void DirWalk::walk(const string& dirname)
{
	_p->_dirname = dirname;
	_p->_dirname.append("*.*");
	if (_p->_status == 1)
	{
		_findclose(_p->_handle);
		_p->_status = 0;
	}
}

int DirWalk::next(string& filename,int& is_dir)
{
	_finddata_t _f;
	if (_p->_status == 0) /// Not started
	{
		_p->_handle = _findfirst(_p->_dirname.c_str(), &_f);
		if (_p->_handle < 0)
		{
			return -1; /// Error
		}
		_p->_status = 1;
	}
	else if (_p->_status == 1) // Looping
	{
		if (_findnext(_p->_handle, &_f) < 0)
		{
			_findclose(_p->_handle);
			_p->_status = 2; // switch to 2 to stop infinite loop.
			return 0;
		}
	}
	else // End of Loop
	{
		return 0;
	}

	if (_f.attrib & _A_SUBDIR)
	{
		if (strcmp(_f.name, ".") != 0
			&& strcmp(_f.name, "..") != 0)
		{
			filename = _f.name;
			is_dir = 1;
		}
		else
		{
			// Loop to next
			return next(filename, is_dir);
		}
	}
	else
	{
		filename = _f.name;
		is_dir = 0;
	}

	printf("DirOP: %s %d\n", filename.c_str(), is_dir);
	return 1; // If filename and is_dir is set correctly, return 1.
}
#endif // End of ifdef _WIN32
