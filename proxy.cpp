#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
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
		
	    struct ifaddrs * ifAddrStruct=NULL;
		struct ifaddrs * ifa=NULL;
		void * tmpAddrPtr=NULL;

		//Get a linked list of all local addresses
		getifaddrs(&ifAddrStruct);
		
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		  (char *)&serv_addr.sin_addr.s_addr,
		  server->h_length);
		serv_addr.sin_port = htons(remoteServerPort);
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		
		int connected = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
		
		bool onLocal = false;
		bool useShared = false;
		
		//See if the http server is on the local machine
		//http://stackoverflow.com/questions/212528/linux-c-get-the-ip-address-of-local-computer
		for(ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
		{
			for(int i = 0; server->h_addr_list[i] != NULL; i++)
				if(memcmp(server->h_addr_list[i], ifa->ifa_addr->sa_data, 14))
					onLocal = true;
		}
		
		//Determine if the local server is ours
		if(onLocal)
		{
			//unregister the server in shared memory
			for(int i = 0; i < MAXSERVERS; i++)
				if(serverList[i] == remoteServerPort)
				{
					useShared = true;
				}
		}
			
		
		string req = "";
#ifdef SHMEM
		if(useShared)
			req = "SHBUFF " + fileName + " HTTP/1.0\r\n\r\n";
		else
#endif
		req = "GET " + fileName + " HTTP/1.0\r\n\r\n";
		
		int err = write(sockfd, req.c_str(), strlen(req.c_str()));
		
		//Read back from the socket
		char buf[SENDSIZE];
		string contents = "";
		
#ifdef SHMEM
		int shIdx = 0;
		if(useShared)
		{
			//Get the shared memory index
			err = read(sockfd, buf, sizeof(buf));
			bcopy(buf, &shIdx, sizeof(int));
			//cout << "ShMem: " << shIdx << endl;
		}
#endif
		
		do
		{
#ifdef SHMEM
			if(useShared)
			{
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
			}
			else
#endif
			err = read(sockfd, buf, sizeof(buf));
			
			if(err > 0) contents += string(buf, err);
			
		}while(err > 0);
		
		//cout << contents << endl;
		
		//Now write the contents back to the client
		write(socketNum, contents.c_str(), strlen(contents.c_str()));
		
	}
};
}
