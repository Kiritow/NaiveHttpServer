import os
from sys import argv

def BuildIfN(filename):
    print('Considering '+filename+'...')
    object_name=''
    if(filename.endswith('.cpp')):
        object_name=filename.replace('.cpp','.o')
    elif(filename.endswith('.c')):
        object_name=filename.replace('.c','.o')

    if((not os.path.exists(object_name)) or (os.stat(filename).st_mtime>os.stat(object_name).st_mtime) ):
        build_cmd='g++ -std=c++14 -c '+filename+' -o '+object_name
        print(build_cmd)
        if(os.system(build_cmd)!=0):
            raise Exception('Failed to build '+filename,filename,object_name)

    return object_name

def BuildAll(lst):
    klst=[]
    for s in lst:
        klst.append(BuildIfN(s))
    cmd='g++ '
    for s in klst:
        cmd=cmd+s+' '
    cmd=cmd+' -ldl -lpthread -o main'
    print(cmd)
    os.system(cmd)

def ScanSource(dirname):
    lst=[]
    for par,dirs,files in os.walk(dirname):
        for f in files:
            if(f.endswith('.cpp') or f.endswith('.c')):
                lst.append(os.path.join(par,f))
    return lst

def CleanObject(source_list):
    for f in source_list:
        if(f.endswith('.c')):
            t=f.replace('.c','.o')
            print('Removing '+t+'...')
            try:
                os.remove(t)
            except:
                print('Failed to delete '+t)
        elif(f.endswith('.cpp')):
            t=f.replace('.cpp','.o')
            print('Removing '+t+'...')
            try:
                os.remove(t)
            except:
                print('Failed to remove '+t)
slst=ScanSource('.')
if(len(argv)>1 and argv[1]=='clean'):
    CleanObject(slst)
else:
    BuildAll(slst)

