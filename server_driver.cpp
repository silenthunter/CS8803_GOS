#include "server.cpp"

int main(int argc, char* argv[])
{
	HTTP_Server srv(25000);
	//srv.beginAcceptLoop();
	srv.setupThreadPool(5);
	//srv.running = false;
	sleep(5);
}
