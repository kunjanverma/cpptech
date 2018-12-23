#include <log.h>
#include <thread>

extern std::string APP_NAME;
extern void waitForLogLvl(int listenPort);

#define NO_TRACE

int main()
{
	std::thread log_lvl_th(waitForLogLvl, 43555);

	while(1)
	{
		TRACE("MSG");
		INFO("MSG");
		WARN("MSG");
		ERROR("MSG");
		FATAL("MSG");
		std::this_thread::sleep_for (std::chrono::seconds(1));
	}
	return 0;
}
