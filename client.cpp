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
	pthread_cond_t *finishedCondition;
	pthread_mutex_t *finishedLock;
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
		
		finishedCondition = PTHREAD_COND_INITIALIZER;
		finishedLock = PTHREAD_MUTEX_INITIALIZER;
	}
	
	/**
	 * @brief the main thread that will spawn worker threads
	 * to connect to the HTTP server
	 * 
	 * @param threadCount The number of threads to spawn
	 * @param loopLimit How many times each worker thread will run
	 */
	void runWorkerThreads(int threadCount, int loopLimit)
	{
		pthread_t workerThreads[threadCount];
		workerThreadStruct wrkData;
		wrkData.port = port;
		wrkData.loopLimit = loopLimit;
		wrkData.runningThreads = &runningThreads;
		wrkData.finishedCondition = &finishedCondition;
		wrkData.finishedLock = &finishedLock;
		
		runningThreads = 0;
		
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
			pthread_join(workerThreads[i], NULL);
		}
		
		//get the end time
		gettimeofday(&end, NULL);
		
		long  seconds  = end.tv_sec  - start.tv_sec;
		long useconds = end.tv_usec - start.tv_usec;

		long mtime = (seconds) * 1000000 + useconds;
		
		cout << mtime << endl;

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
		
		for(int i = 0; i < data->loopLimit; i++)
		{
	
			int sockfd = socket(AF_INET, SOCK_STREAM, 0);
			hostent *server = gethostbyname("127.0.0.1");
			
			struct sockaddr_in serv_addr;
			
			//ASSERT_NE(server, (hostent*)0x0);
			
			bzero((char *) &serv_addr, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			bcopy((char *)server->h_addr,
			  (char *)&serv_addr.sin_addr.s_addr,
			  server->h_length);
			serv_addr.sin_port = htons(data->port);
			
			int connected = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
			//EXPECT_EQ(connected, 0);
			
			string req = "GET test.txt HTTP/1.0\r\n\r\n";
			
			write(sockfd, req.c_str(), strlen(req.c_str()));
			
			//Read the response
			char buffer[255];
			string input;
			int bytesRead = 1;
			while(bytesRead > 0)
			{
				bytesRead = read(sockfd, &buffer, 255);
				input += string(buffer, 0, bytesRead);
			}
			
			//cout << input << endl;
			
			shutdown(sockfd, 2);
		}
	}
};
