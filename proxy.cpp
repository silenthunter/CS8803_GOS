#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
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
#include "imageAlter.h"
//#include "client.cpp"

namespace{
class HTTP_Proxy : public virtual HTTP_Server
{
	private:
	
	int remoteServerPort;
	CLIENT* clnt;
	
	public:
	HTTP_Proxy(int recvPort, int acceptQueueSize, int remotePort) 
		: HTTP_Server(recvPort, acceptQueueSize)
	{
		remoteServerPort = remotePort;
		clnt = clnt_create("localhost", IMAGEALTERPROG, IMAGEALTERVERS, "tcp");
		if (clnt == NULL) {
			clnt_pcreateerror ("localhost");
			exit (1);
		}
	}
	
	virtual void parseHTTPRequest(string fileName, int socketNum, DataMethod method, string host, int altPort)
	{
		struct sockaddr_in serv_addr;
		hostent *server = host.length() == 0 ? gethostbyname("127.0.0.1") : gethostbyname(host.c_str());
		
		struct ifaddrs * ifAddrStruct=NULL;
		struct ifaddrs * ifa=NULL;
		void * tmpAddrPtr=NULL;
		
		//Determine the port to use
		int destPort = altPort ? altPort : 80;
		
		if(host.length() == 0) destPort = remoteServerPort;

		//Get a linked list of all local addresses
		getifaddrs(&ifAddrStruct);
		
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		  (char *)&serv_addr.sin_addr.s_addr,
		  server->h_length);
		serv_addr.sin_port = htons(destPort);
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);

		//Set timeout
		struct timeval timeout;      
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
			sizeof(timeout)) < 0)
			printf("setsockopt failed\n");

		if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
			sizeof(timeout)) < 0)
			printf("setsockopt failed\n");

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
		
		delete ifAddrStruct;
		
		string req = "";
		req = "GET " + fileName + " HTTP/1.0\r\n";
		if(host.length() > 0) req += "Host:" + host + "\r\n";
		req += "\r\n";
		
		int err = write(sockfd, req.c_str(), strlen(req.c_str()));
		
		//Read back from the socket
		unsigned char buf[SENDSIZE];
		unsigned char *contents = 0x0;
		unsigned long contSize = 1;
		unsigned long recv = 0;
		
		do
		{
			err = read(sockfd, buf, sizeof(buf));
			
			if(err > 0) 
			{
				while(recv + err >= contSize)
				{
					contSize *= 2;
					contents = (unsigned char*)realloc(contents, contSize);
				}
				//contents += string(buf, err);
				bcopy(buf, contents + recv, err);
				recv += err;
			}
			
		}while(err > 0);
		
		//cout << contents << endl;
		if(strstr((char*)contents, "200 OK") && ((fileName.rfind("jpeg") == fileName.length() - 4) ||
		(fileName.rfind("jpg") == fileName.length() - 3)))
		{
			int idx;
			for(idx = 4; idx < recv - 1 && !(contents[idx - 3] == '\r' && contents[idx - 2] == '\n' && contents[idx - 1] == '\r' && contents[idx] == '\n'); idx++);
			idx++;
			dataStruct dat;
			dataStruct *result;
			dat.data = contents + idx;
			dat.len = recv - idx;

			result = imagealter_1(&dat, clnt);
			if(result == NULL)
				clnt_perror(clnt, "call failed");
			else
				bcopy(result->data, contents + idx, result->len);
		}
		
		//Now write the contents back to the client
		write(socketNum, contents, recv);
		
	}
};
}
