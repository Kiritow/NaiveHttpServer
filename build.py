import os
from hashlib import md5
from sys import argv

_source_lst=[]

def GetMD5(filename):
    f=open(filename,'rb')
    h=md5()
    while True:
        b=f.read(4096)
        if not b:
            break
        h.update(b)
    f.close()
    return h.hexdigest()

def IsNewSource(filename):
    global _source_lst

    check_code=GetMD5(filename)
    if(check_code in _source_lst):
        return False,check_code
    else:
        return True,check_code

def AddNewSource(filename):
    global _source_lst

    flg,check_code=IsNewSource(filename)
    if(flg):
        _source_lst.append(check_code)
        return True
    else:
        return False

def BuildSingle(filename,compile_option):
    print('Considering '+filename+'...')
    object_name=''

    if(not AddNewSource(filename)):
        print('Source has been compiled before. '+filename)
        return False,object_name

    compiler='g++ -ILogger -std=c++14'

    if(filename.endswith('.cpp')):
        object_name=filename.replace('.cpp','.o')
    elif(filename.endswith('.c')):
        object_name=filename.replace('.c','.o')
        compiler='gcc'

    if((not os.path.exists(object_name)) or (os.stat(filename).st_mtime>os.stat(object_name).st_mtime) ):
        build_cmd=compiler+' -fPIC '+compile_option+' -c '+filename+' -o '+object_name
        print(build_cmd)
        if(os.system(build_cmd)!=0):
            raise Exception('Failed to build '+filename,filename,object_name)

    return True,object_name

def BuildAll(lst,compile_option='',link_option=''):
    klst=[]
    for s in lst:
        flg,objname=BuildSingle(s,compile_option)
        if(flg):
            klst.append(objname)
    cmd='g++ '
    for s in klst:
        cmd=cmd+s+' '
    cmd=cmd+' -fPIC -ldl -lpthread '+link_option+'-o main'
    print(cmd)
    os.system(cmd)

def ScanSource(dirname):
    lst=[]
    for par,dirs,files in os.walk(dirname):
        for f in files:
            if(f.endswith('.cpp') or f.endswith('.c')):
                lst.append(os.path.join(par,f))
    return lst

def RemoveFileES(filename):
    # ES: Exception-Safe
    try:
        os.remove(filename)
    except:
        print('Failed to remove '+filename)

def CleanObject(source_list):
    print('Removing main...')
    RemoveFileES('main')
    for f in source_list:
        if(f.endswith('.c')):
            t=f.replace('.c','.o')
            print('Removing '+t+'...')
            RemoveFileES(t)
        elif(f.endswith('.cpp')):
            t=f.replace('.cpp','.o')
            print('Removing '+t+'...')
            RemoveFileES(t)

def build():
    slst=ScanSource('.')
    if(len(argv)>1 and argv[1]=='clean'):
        CleanObject(slst)
    else:
        if(len(argv)==1):
            BuildAll(slst)
        elif(len(argv)==2):
            BuildAll(slst,argv[1])
        else:
            BuildAll(slst,argv[1],argv[2])

if __name__=='__main__' :
    build()
