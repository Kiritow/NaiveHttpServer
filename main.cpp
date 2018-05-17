#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include "GSock/gsock.h"
#include "NaiveThreadPool/ThreadPool.h"
using namespace std;

int recvline(sock& s,std::string& out)
{
        std::string ans;
        char buff[16];
        while(true)
        {
                buff[0]=buff[1]=0;
                int ret=s.recv(buff,1);
                if(ret<0) return ret;
                ans.push_back(buff[0]);
                if(ans.find("\r\n")!=std::string::npos)
                {
                        break;
                }
        }
        out=ans;
        return ans.size();
}

int recvheader_raw(sock& s,std::string& header_raw)
{
        std::string ans;
        while(true)
        {
                std::string tmp;
                int ret=recvline(s,tmp);
                if(ret<0) return ret;
                ans.append(tmp);
                if(ans.find("\r\n\r\n")!=std::string::npos)
                {
                        break;
                }
        }
        header_raw=ans;
        return ans.size();
}

int request_handler(sock& s)
{
        string header_raw;
        int ret=recvheader_raw(s,header_raw);
        if(ret<0)
        {
                return ret;
        }
        else
        {
                printf("%s\n",header_raw.c_str());
                string simple_response="HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                s.send(simple_response.c_str(),simple_response.size());
                return 0;
        }
}

#define dprintf printf
#define BIND_PORT 8000

int main()
{
        serversock t;
        if(t.bind(BIND_PORT)<0)
        {
                dprintf("Failed to bind at port %d\n",BIND_PORT);
                return 0;
        }
        if(t.listen(10)<0)
        {
                dprintf("Failed to listen at port %d\n",BIND_PORT);
        }
        dprintf("Server started at port %d\n",BIND_PORT);
        dprintf("Starting thread pool...\n");
        ThreadPool tp(10);
        dprintf("Server is now ready for connections.\n");
        while(true)
        {
                sock* ps=new sock;
                int ret=t.accept(*ps);
                if(ret<0)
                {
                        dprintf("Failed to accept connection. Abort.\n");
                        break;
                }
                if(tp.start([ps](){
                                        int ret=request_handler(*ps);
                                        dprintf("request handler returns %d\n",ret);
                                        delete ps;
                                })<0)
                {
                        dprintf("Failed to start job at thread pool.\n");
                }
                else
                {
                        dprintf("Job started with sock: %p\n",ps);
                }
        }
        dprintf("Server closed.\n");
}

