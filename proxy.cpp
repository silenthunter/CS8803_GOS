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
#include <sys/shm.h>
#include <sys/ipc.h>
#include "server.cpp"
#include "client.cpp"

namespace{
class HTTP_Proxy : public virtual HTTP_Server
{
	private:
	
	int remoteServerPort;
	
	public:
	HTTP_Proxy(int recvPort, int acceptQueueSize, int remotePort) 
		: HTTP_Server(recvPort, acceptQueueSize)
	{
		remoteServerPort = remotePort;
	}
	
	virtual void parseHTTPRequest(string fileName, int socketNum, DataMethod method)
	{		
		struct sockaddr_in serv_addr;
		hostent *server = gethostbyname("127.0.0.1");
		
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		  (char *)&serv_addr.sin_addr.s_addr,
		  server->h_length);
		serv_addr.sin_port = htons(remoteServerPort);
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		
		int connected = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
			
#ifdef SHMEM
		string req = "SHBUFF " + fileName + " HTTP/1.0\r\n\r\n";
#else
		string req = "GET " + fileName + " HTTP/1.0\r\n\r\n";
#endif
		
		int err = write(sockfd, req.c_str(), strlen(req.c_str()));
		
		//Read back from the socket
		char buf[SENDSIZE];
		string contents = "";
		
#ifdef SHMEM
		//Get the shared memory index
		err = read(sockfd, buf, sizeof(buf));
		int shIdx = 0;
		bcopy(buf, &shIdx, sizeof(int));
		//cout << "ShMem: " << shIdx << endl;
#endif
		
		do
		{
#ifdef SHMEM
			//cout << "before" << endl;
			pthread_mutex_lock((pthread_mutex_t*)(shMem[shIdx] + 1));
			
			char* cond = (char*)(shMem[shIdx] + 1) + sizeof(pthread_mutex_t);
			
			while(*(shMem[shIdx]) != MODIFIED)
			{
				//cout << "waiting" << endl;
				pthread_cond_wait((pthread_cond_t*) cond, (pthread_mutex_t*)(shMem[shIdx] + 1));
			}
			
			char* mutexEnd = (char*)(shMem[shIdx] + 1) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t);
			char* dataStart = mutexEnd + sizeof(int);
			
			bcopy(mutexEnd, &err, sizeof(int));
			bcopy(dataStart, buf, err);
			
			//Mark the buffer as being read
			*(shMem[shIdx]) = READ;
			
			//cout << "read" << endl;
			
			pthread_cond_signal((pthread_cond_t*)cond);
			
			pthread_mutex_unlock((pthread_mutex_t*)(shMem[shIdx] + 1));
			
			//cout << "after" << endl;
#else
			err = read(sockfd, buf, sizeof(buf));
#endif
			if(err > 0) contents += string(buf, err);
			
		}while(err > 0);
		
		//cout << contents << endl;
		
		//Now write the contents back to the client
		write(socketNum, contents.c_str(), strlen(contents.c_str()));
		
	}
};
}
