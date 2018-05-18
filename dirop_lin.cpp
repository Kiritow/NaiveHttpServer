#ifndef _WIN32 // Linux 
#include "dirop.h"
#include <dirent.h>
using namespace std;

struct DirWalk::_impl
{
        DIR* dir;
};

DirWalk::DirWalk() : _p(new _impl)
{
        _p->dir=NULL;
}

DirWalk::DirWalk(const string& path)
{
        walk(path);
}

DirWalk::~DirWalk()
{
        if(_p->dir)
        {
                closedir(_p->dir);
        }
        delete _p;
}

void DirWalk::walk(const string& path)
{
        _p->dir=opendir(path.c_str());
}

int DirWalk::next(string& filename,int& is_dir)
{
        struct dirent* file=NULL;
        file=readdir(_p->dir);
        if(file==NULL)
        {
                return 0;
        }
        if(file->d_type==DT_DIR)
        {
                if(strcmp(file->d_name,".")==0 || strcmp(file->d_name,"..")==0)
                {
                        return next(filename,is_dir);
                }
                filename=file->d_name;
                is_dir=1;
                return 1;
        }
        else
        {
                filename=file->d_name;
                is_dir=0;
                return 1;
        }
}

#endif // End of ifndef _WIN32
