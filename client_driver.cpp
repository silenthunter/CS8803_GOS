#include "client.cpp"

int main(int argc, char* argv[])
{
	client c(atoi(argv[2]));
	c.runWorkerThreads(argv[1], argv[3], atoi(argv[4]), atoi(argv[5]), argc >=6 ? argv[6] : "");
}
