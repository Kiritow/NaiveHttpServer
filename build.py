import os
from sys import argv

def BuildIfN(filename,compile_option):
    print('Considering '+filename+'...')
    object_name=''

    compiler='g++ -std=c++14'

    if(filename.endswith('.cpp')):
        object_name=filename.replace('.cpp','.o')
    elif(filename.endswith('.c')):
        object_name=filename.replace('.c','.o')
        compiler='gcc'

    if((not os.path.exists(object_name)) or (os.stat(filename).st_mtime>os.stat(object_name).st_mtime) ):
        build_cmd=compiler+' -fPIC -fPIE '+compile_option+' -c '+filename+' -o '+object_name
        print(build_cmd)
        if(os.system(build_cmd)!=0):
            raise Exception('Failed to build '+filename,filename,object_name)

    return object_name

def BuildAll(lst,compile_option='',link_option=''):
    klst=[]
    for s in lst:
        klst.append(BuildIfN(s,compile_option))
    cmd='g++ '
    for s in klst:
        cmd=cmd+s+' '
    cmd=cmd+' -fPIC -fPIE -ldl -lpthread '+link_option+'-o main'
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
