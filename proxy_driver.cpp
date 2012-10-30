#include "proxy.cpp"

int main(int argc, char* argv[])
{
	HTTP_Proxy p(8000, 5, 8001);
	p.setupThreadPool(5);
	p.beginAcceptLoop();
	
	sleep(500);
}
