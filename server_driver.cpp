#include "server.cpp"

int main(int argc, char* argv[])
{
	HTTP_Server srv(atoi(argv[1]), atoi(argv[2]));
	srv.setupThreadPool(atoi(argv[3]));
	srv.beginAcceptLoop();

	while(1)
		sleep(10);
}
