#include <log.h>

extern void waitForLogLvl(int app_num, int listenPort);

#define NO_TRACE

int main(int argc, char *argv[])
{
	std::thread log_lvl_th(waitForLogLvl, atoi(argv[1]), atoi(argv[2]));

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
