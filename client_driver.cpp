#include "client.cpp"

int main(int argc, char* argv[])
{
	client c(atoi(argv[1]));
	c.runWorkerThreads(argv[2], atoi(argv[3]), atoi(argv[4]));
}
