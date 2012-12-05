#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

using namespace std;

struct workerThreadStruct
{
	int loopLimit;
	int port;
	int* runningThreads;
	char* file;
	const char* host;
	pthread_cond_t *finishedCondition;
	pthread_mutex_t *finishedLock;
	sockaddr_in* sockAddress;
};

struct threadReturn
{
	int error;
	long recv;
};

/**
 * @brief Allows for connection to and requesting pages from an HTTP Server
 */
class client
{
	private:
	///The port the remote server listens on
	int port;
	
	///Pthreads condition that will broadcast when all clients have been created
	pthread_cond_t finishedCondition;
	
	///The mutex that synchronizes the boss thread and the worker threads
	pthread_mutex_t finishedLock;
	
	///The number of threads that have been initialized
	int runningThreads;
	
	public:
	
	/**
	 * @brief Basic constructor that will initialize the HTTP client
	 * 
	 * @param remotePort The port the remote server listens on
	 */
	client(int remotePort)
	{
		port = remotePort;
		
		pthread_mutex_init(&finishedLock, NULL);
		pthread_cond_init(&finishedCondition, NULL);
	}
	
	/**
	 * @brief the main thread that will spawn worker threads
	 * to connect to the HTTP server
	 * 
	 * @param The address of the server you are connecting to
	 * @param threadCount The number of threads to spawn
	 * @param loopLimit How many times each worker thread will run
	 * @param The host a proxy should redirect to
	 */
	void runWorkerThreads(char* dest, char* file, int threadCount, int loopLimit, const char* host)
	{
		pthread_t workerThreads[threadCount];
		
		workerThreadStruct wrkData;
		wrkData.port = port;
		wrkData.loopLimit = loopLimit;
		wrkData.runningThreads = &runningThreads;
		wrkData.file = file;
		wrkData.host = host;
		wrkData.finishedCondition = &finishedCondition;
		wrkData.finishedLock = &finishedLock;

		hostent *server = gethostbyname(dest);
		
		struct sockaddr_in serv_addr;
		
		//ASSERT_NE(server, (hostent*)0x0);
		
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		  (char *)&serv_addr.sin_addr.s_addr,
		  server->h_length);
		serv_addr.sin_port = htons(port);

		wrkData.sockAddress = &serv_addr;
		
		runningThreads = 0;
		
		threadReturn* retn[threadCount];
		
		for(int i = 0; i < threadCount; i++)
		{
			workerThreads[i] = pthread_t();
			pthread_create(&workerThreads[i], NULL, client::workerThread, &wrkData);
		}
		
		//spin wait for the threads
		while(runningThreads < threadCount);
		
		//Start the timer
		struct timeval start, end;
		gettimeofday(&start, NULL);
		
		pthread_cond_broadcast(&finishedCondition);
		
		//rejoin
		for(int i = 0; i < threadCount; i++)
		{
			pthread_join(workerThreads[i], (void**)&(retn[i]));
		}
		
		//get the end time
		gettimeofday(&end, NULL);
		
		//count disconnects
		int errors = 0;
		long bytesTransferred = 0;
		for(int i = 0; i < threadCount; i++)
		{
			errors += retn[i]->error;
			bytesTransferred += retn[i]->recv;
			delete retn[i];
		}
		
		long  seconds  = end.tv_sec  - start.tv_sec;
		long useconds = end.tv_usec - start.tv_usec;

		long mtime = (seconds) * 1000000 + useconds;
		
		cout << mtime << "\t" << errors << endl;
		cout << "Bytes transferred: " << bytesTransferred << endl;

	}
	
	/**
	 * @brief The worker thread that will actually connect
	 *  and write to the HTTP server
	 * 
	 * @param dataStruct The data structure that contains loop
	 * and connection information
	 */
	static void *workerThread(void* dataStruct)
	{
		workerThreadStruct* data = (workerThreadStruct*) dataStruct;
		
		//Wait for permission to continue
		pthread_mutex_lock(data->finishedLock);
		(*data->runningThreads)++;
		pthread_cond_wait( data->finishedCondition, data->finishedLock );
		pthread_mutex_unlock(data->finishedLock);
		
		threadReturn* retn = new threadReturn;
		retn->error = 0;
		retn->recv = 0;
		
		for(int i = 0; i < data->loopLimit; i++)
		{
	
			int sockfd = socket(AF_INET, SOCK_STREAM, 0);
			
			int connected = connect(sockfd,(struct sockaddr *)data->sockAddress,sizeof(*(data->sockAddress)));
			//EXPECT_EQ(connected, 0);

		    struct timeval timeout;      
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
				sizeof(timeout)) < 0)
				printf("setsockopt failed\n");

			if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
			    	sizeof(timeout)) < 0)
			    	printf("setsockopt failed\n");

				
			string req = "GET " + string(data->file) + " HTTP/1.0\r\n";
			if(data->host)
				req += "Host: " + string(data->host) + "\r\n";
			req += "\r\n";
			
			int err = write(sockfd, req.c_str(), strlen(req.c_str()));
			
			//Read the response
			char buffer[255];
			string input;
			int bytesRead = 1;
			while(bytesRead > 0)
			{
				bytesRead = read(sockfd, &buffer, 255);
				input += string(buffer, 0, bytesRead);
				retn->recv += bytesRead;
			}
			
			//An error occured
			if(bytesRead < 0 || input.length() == 0)
			{
				//cout << "Errno: " << errno << endl;
				retn->error++;
				close(sockfd);
				//cout << "Was full" << endl;
				continue;
			}
			
			//cout << input << endl;
			
			close(sockfd);
		}
		
		pthread_exit(retn);
	}
};
