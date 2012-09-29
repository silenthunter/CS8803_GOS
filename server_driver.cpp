#include "server.cpp"

int main(int argc, char* argv[])
{
	HTTP_Server srv(25000, 10);
	srv.setupThreadPool(5);
	srv.beginAcceptLoop();
	//srv.running = false;
	sleep(600);
}
