#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <queue>
#include <fstream>
#include <signal.h>
#include <errno.h>
#include "server.cpp"

class HTTP_Proxy : public virtual HTTP_Server
{
	public:
	HTTP_Proxy(int recvPort, int acceptQueueSize, int remotePort) 
		: HTTP_Server(recvPort, acceptQueueSize)
	{
	}
	
	virtual void parseHTTPRequest(string fileName, int socketNum)
	{
		cout << "Test" << endl;
	}
};
