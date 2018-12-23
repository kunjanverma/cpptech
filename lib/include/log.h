#ifndef __LOG_H__
#define __LOG_H__

#include <cstdlib>
#include <cstdarg>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <time.h>
#include <string>
#include <string.h>
#include <iostream>

static std::mutex logMutex;
static std::mutex dbg_lvl_mtx;

enum DEBUG_LEVEL
{
	LOG_FATAL=1,
	LOG_ERROR=2,
	LOG_WARN =3,
	LOG_INFO =4,
	LOG_TRACE=5
};

#ifndef NO_TRACE
static int debugLvl=LOG_INFO;
#else
static int debugLvl=LOG_TRACE;
#endif

void waitForLogLvl(int listenPort)
{
	int sockfd, clientsock;
	char str_dbg_lvl[2];
	struct sockaddr_in saClient, saServer;

	if( (sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))==-1 )
	{
		std::lock_guard<std::mutex> lck (logMutex);
		std::cerr << __FILE__ << "[" << __LINE__ << "]" << "Error opening socket for logging\n";
		return;
	}
	memset( &saServer, 0, sizeof( saServer ) );
	saServer.sin_family = AF_INET;
    saServer.sin_port = htons( listenPort );
    saServer.sin_addr.s_addr = INADDR_ANY;
	
	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), &enable, sizeof(int)) < 0)
	{
		std::lock_guard<std::mutex> lck (logMutex);
    	std::cerr << __FILE__ << "[" << __LINE__ << "]" << "setsockopt(SO_REUSEADDR) failed"<<errno<<"\n";
		return;
	}

	if(::bind( sockfd, (struct sockaddr*)&saServer, sizeof(saServer) ) < 0 )
    {
		std::lock_guard<std::mutex> lck (logMutex);
        std::cerr << __FILE__ << "[" << __LINE__ << "]" << "Unable to bind socket\n";
        return;
    }

	::listen( sockfd, 1 );
	
	while(true)
	{
		unsigned int sz = sizeof(saServer);
		clientsock = ::accept(sockfd, (struct sockaddr*)&saServer, &sz);

		if(clientsock<0)
		{
			std::lock_guard<std::mutex> lck (logMutex);
        	std::cerr << __FILE__ << "[" << __LINE__ << "]" << "accept failed\n";
        	return;
		}
	
		recv(clientsock, str_dbg_lvl,1,0);
		str_dbg_lvl[1]=0;
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);
		int val = strtol(str_dbg_lvl,NULL,10);
		debugLvl = val==0?debugLvl:val;
	}
}

void gettimestr(char *timestr)
{
	struct timespec ts;
	static char buffer[16];

	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm * ptm = localtime(&ts.tv_sec);
	strftime(buffer, 16, "%h %d %T", ptm);

	sprintf(timestr, "%s.%ld\n", buffer,ts.tv_nsec);
}

inline bool ixecuteLog( bool istrace, const char* mode, const char* file, int line, const char* func, const char* fmt, ...)
{
	std::lock_guard<std::mutex> lck (logMutex);
    const int bufLen = 8196;
	static char timestr[26];
	gettimestr(timestr);
    static char cbuffer[bufLen];
	va_list argList;
	va_start(argList, fmt);
    vsnprintf( cbuffer, bufLen, fmt, argList );
    va_end( argList );
#ifdef LOG_TO_STDERR
    std::cerr << timestr << " " << mode << " " << file << " [" << line << "] " << func << "(): " 
			  << cbuffer << std::endl;
#else   // stdout
	if(istrace)    // add thread ID field to debug builds
	{
    	std::cout << timestr << " " << mode << " " << "{" << std::this_thread::get_id() << "} " 
				  << file << " [" << line << "] " << func << "(): " << cbuffer << std::endl;
	}
	else
	{
    	std::cout << timestr << " " << mode << " " << file << " [" << line << "] " << func << "(): " 
				  << cbuffer << std::endl;
	}
#endif
	return true;
}

#define TRACE(fmt, ...)	{									\
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);		\
		(debugLvl == LOG_TRACE) && 							\
	    ixecuteLog( true, "TRC", __BASE_FILE__, __LINE__,__FUNCTION__,fmt, ##__VA_ARGS__); \
}
#define INFO(fmt, ...)	{									\
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);		\
		(debugLvl >= LOG_INFO) && 							\
	    ixecuteLog( false, "INF", __BASE_FILE__, __LINE__,__FUNCTION__,fmt, ##__VA_ARGS__); \
}
#define WARN(fmt, ...)	{									\
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);		\
		(debugLvl >= LOG_WARN) && 							\
	    ixecuteLog( false, "WRN", __BASE_FILE__, __LINE__,__FUNCTION__,fmt, ##__VA_ARGS__); \
}
#define ERROR(fmt, ...)	{									\
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);		\
		(debugLvl >= LOG_ERROR) && 							\
	    ixecuteLog( false, "ERR", __BASE_FILE__, __LINE__,__FUNCTION__,fmt, ##__VA_ARGS__); \
}
#define FATAL(fmt, ...)	{									\
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);		\
		(debugLvl >= LOG_FATAL) && 							\
	    ixecuteLog( false, "FTL", __BASE_FILE__, __LINE__,__FUNCTION__,fmt, ##__VA_ARGS__); \
}
#endif
