#ifndef __LOG_H__
#define __LOG_H__

#include <cstdarg>
#include <mutex>
#include <arpa/inet.h>
#include <thread>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>

static std::mutex logMutex;
static std::mutex dbg_lvl_mtx;
/*
socat UDP4-RECVFROM:43555,ip-add-membership=226.1.1.1:localhost,fork STDOUT
netcat -u -s 127.0.0.1 226.1.1.1 43555
sudo tcpdump -nnXs 0 -i lo udp port 43555 and dst 226.1.1.1
*/

/*16 bit hex string is required. 3 LSB bits denotes the log level. Rest 13 bits are for 13 different applications.*/
#define LOG_LEVEL_MASK 0x0007 // First 3 bits for log level
#define LOG_LEVEL_NUM_BITS 3
#define APP_INDEX_INVALID 14 //Cannot have more than 13 application using logging service on same port

enum DEBUG_LEVEL
{
	LOG_FATAL=1,
	LOG_ERROR=2,
	LOG_WARN =3,
	LOG_INFO =4,
	LOG_TRACE=5,
	LOG_LVL_INVALID=6
};

#ifndef NO_TRACE
static int debugLvl = LOG_INFO;
#else
static int debugLvl = LOG_TRACE;
#endif

void waitForLogLvl(int app_num, int listenPort)
{
	if(APP_INDEX_INVALID <= app_num)
	{
		std::lock_guard<std::mutex> lck (logMutex);
    	std::cerr << __FILE__ << "[" << __LINE__ << "]" << " Error: Wrong App number"<<"\n";
		return;
	}

	int sockfd;
	struct ip_mreq group;
	char str_dbg_inp[7]; /*format expected is 0x0000 - 0xffff*/
	struct sockaddr_in saServer;

	if( (sockfd=socket(AF_INET, SOCK_DGRAM, 0))==-1 )
	{
		std::lock_guard<std::mutex> lck (logMutex);
		std::cerr << "INFO " << __FILE__ << "[" << __LINE__ << "]" << 
					 "Some other app using this log service is running\n";
	}
	int enable = 1;
	if( setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),  &enable, sizeof(int)) < 0)
	{
		std::lock_guard<std::mutex> lck (logMutex);
    	std::cerr << __FILE__ << "[" << __LINE__ << "]" << "setsockopt(SO_REUSEADDR) failed"<<errno<<"\n";
		close(sockfd);
		return;
	}
	memset( &saServer, 0, sizeof( saServer ) );
	saServer.sin_family = AF_INET;
    saServer.sin_port = htons( listenPort );
    saServer.sin_addr.s_addr = htonl(INADDR_ANY);
    //saServer.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	if( ::bind( sockfd, (struct sockaddr*)&saServer, sizeof(saServer) ) < 0 )
    {
		std::lock_guard<std::mutex> lck (logMutex);
        std::cerr << __FILE__ << "[" << __LINE__ << "]" << "Unable to bind socket\n";
		close(sockfd);
        return;
    }
	group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
	group.imr_interface.s_addr = htonl(INADDR_ANY);
	//group.imr_interface.s_addr = inet_addr("127.0.0.1");
	if(setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
	{
			std::cerr << __FILE__ << "[" << __LINE__ << "]" << "Adding multicast group error\n";
			close(sockfd);
			return;
	}

	/*unsigned char do_enable = (unsigned char) enable;
	if(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &do_enable, sizeof(do_enable)) < 0)
	{
			std::cerr << __FILE__ << "[" << __LINE__ << "]" << "Enabling multicast loop failed\n";
			close(sockfd);
			return;
	}*/
	
	while(true)
	{
		read( sockfd, str_dbg_inp, 6 );
		{
			std::lock_guard<std::mutex> lck (logMutex);
			std::cout << "[" << __FUNCTION__ << "]:" << "Recieved string " << str_dbg_inp << " from log socket" << std::endl;
		}

		if ( memcmp("0x", str_dbg_inp, 2) )
		{
			std::lock_guard<std::mutex> lck (logMutex);
        	std::cerr << __FILE__ << "[" << __LINE__ << "]" << "wrong debug level string format\n";
			continue;
		}

		std::stringstream dbg_inp_ss;
		dbg_inp_ss << str_dbg_inp;
		int dbg_inp_recv;
		dbg_inp_ss >> std::hex >> dbg_inp_recv;

		if ( !( (dbg_inp_recv>>LOG_LEVEL_NUM_BITS) & (1 << (app_num-1)) ) ) /*Check if app_num th bit is set for
																		      dbg_inp_recv starting from 
																		      LOG_LEVEL_NUM_BITS th bit*/
		{
			continue; //The data received is not for this application
		}
		
		std::lock_guard<std::mutex> lck(dbg_lvl_mtx);
		int dbg_lvl_recv=dbg_inp_recv & LOG_LEVEL_MASK; 
		debugLvl = dbg_lvl_recv && (dbg_lvl_recv < LOG_LVL_INVALID) ? dbg_lvl_recv : debugLvl;
	}
}

void gettimestr(char *timestr)
{
	struct timespec ts;
	static char buffer[16];

	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm * ptm = localtime(&ts.tv_sec);
	strftime(buffer, 16, "%h %d %T", ptm);

	sprintf(timestr, "%s.%ld", buffer,ts.tv_nsec);
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
    	std::cout << timestr << " " << mode << " " << "{thread_id:" << std::this_thread::get_id() << "} " 
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
