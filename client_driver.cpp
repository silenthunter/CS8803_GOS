#include "client.cpp"

int main(int argc, char* argv[])
{
	client c(25000);
	c.runWorkerThreads(5, 1);
}
