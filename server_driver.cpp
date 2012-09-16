#include "server.cpp"

int main(int argc, char* argv[])
{
	HTTP_Server srv(25000);
	srv.beginAcceptLoop();
}
