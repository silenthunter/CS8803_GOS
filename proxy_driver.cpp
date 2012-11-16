#include "proxy.cpp"

int main(int argc, char* argv[])
{
	HTTP_Proxy p(atoi(argv[1]), atoi(argv[3]), atoi(argv[2]));
	p.setupThreadPool(atoi(argv[4]));
	p.beginAcceptLoop();
	
	while(1)
		sleep(10);
}
