#include <string>
using namespace std;

#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif


string _log_whichThread()
{
	char buff[32] = { 0 };
#ifdef _WIN32
	sprintf(buff, "%ul", GetCurrentThreadId()); // DWORD
#else
	sprintf(buff, "%ul", pthread_self());
#endif
	string ans("Thread ");
	ans.append(buff);
	return ans;
}
