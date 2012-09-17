/**
 * @file server.cpp
 * @author Gavin Gresham <gavin.gresham@cs.gatech.edu>
 * 
 * @section DESCRIPTION
 * Contains the HTTP_Server class
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <queue>

using namespace std;

struct requestData
{
	string filePath;
	int socketNum;
};

/**
 * @brief A multi-threaded server that parses basic HTTP commands
 */
class HTTP_Server
{
	private:
	///The port that the server listens on
	int port;
	
	///The number of clients that can connect before the server must accept
	int queueSize;
	
	///Array of worker threads that will handle client requests
	pthread_t* workerThreads;
	
	///Pthreads condition that will be 'signaled' when a client is accepted
	pthread_cond_t acceptCondition;
	
	///Controls whether the server is running or not
	bool running;
	
	///The mutex that synchronizes the boss thread and the worker threads
	pthread_mutex_t acceptLock;
	
	///Queue that file requests are placed in
	queue<requestData> requestQueue;
	
	//Boolean that tracks the running state of the server
	//static bool serverRunning;
	
	static void error(string errorText)
	{
		cout << errorText << endl;
	}
	

	public:

	/**
	 * @brief Instantiates the HTTP Server object
	 * 
	 * @param port The port the server will bind to
	 */
	HTTP_Server(int serverPort)
	{
		port = serverPort;
		queueSize = 5;
		running = true;
		
		acceptCondition = PTHREAD_COND_INITIALIZER;
		acceptLock = PTHREAD_MUTEX_INITIALIZER;
	}

	/**
	 * @brief Initializes a worker thread pool that will handle the client requests
	 * 
	 * @param poolSize The size of the threadpool that will be initialized
	 */
	void setupThreadPool(int poolSize)
	{
		cout << "Setting up" << endl;
		workerThreads = new pthread_t[poolSize];
		for(int i = 0; i < poolSize; i++)
		{
			workerThreads[i] = pthread_t();
			pthread_create(&workerThreads[i], NULL, HTTP_Server::workerThreadTask, this);
		}
	}

	/**
	 * @brief Starts the boss thread that will accept incoming
	 *  connections and pass them to worker threads.
	 * 
	 * @note Uses the port defined in @ref HTTP_Server(int port)
	 */
	void beginAcceptLoop()
	{
		pthread_t masterThread;
		pthread_create(&masterThread, NULL, HTTP_Server::bossThread, this);
	}
	
	/**
	 * @brief The boss thread that will loop while accepting client 
	 * connections and handing them to worker threads
	 * 
	 * @note http://www.linuxhowtos.org/C_C++/socket.htm
	 */
	static void *bossThread(void* input)
	{
		HTTP_Server* thisSrv = (HTTP_Server*) input;
		int portIN = thisSrv->port;
		struct sockaddr_in serv_addr;
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd < 0)
			error("Unable to open socket");
			
		//Clear the structs
		bzero(&serv_addr, sizeof(serv_addr));
		
		//sets server information
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(portIN);//Host to network
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		
		if(bind(sockfd, (const sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
			error("Bind faild on port: " + portIN);
			
		listen(sockfd, thisSrv->queueSize);
		
		//Loop and accept connections
		while(thisSrv->running)
		{
			struct sockaddr_in *client_addr = new sockaddr_in;
			bzero(&client_addr, sizeof(client_addr));
			socklen_t clientlen = sizeof(*client_addr);
			int newsockfd = accept(sockfd, (struct sockaddr *) client_addr, &clientlen);
		}
		
		pthread_exit(0);
	}
	
	/**
	 * @brief The method worker tasks run that receive client information
	 * from the boss thread
	 */
	static void *workerThreadTask(void* input)
	{
		HTTP_Server* thisSrv = (HTTP_Server*)input;
		
		while(thisSrv->running)
		{
			pthread_mutex_lock(&thisSrv->acceptLock);
			
			//Loop while there aren't any requests
			do
				pthread_cond_wait( &thisSrv->acceptCondition, &thisSrv->acceptLock );
			while(thisSrv->requestQueue.size() > 0);
			
			pthread_mutex_unlock(&thisSrv->acceptLock);
		}
	}
	
	/**
	 * @brief Retrieves the file and returns the string to send back to the client
	 * 
	 * @param fileName The name of the file to be retrieved
	 * 
	 * @return The HTTP formatted response string
	 */
	string parseHTTPRequest(string fileName)
	{
		return "temp";
	}
};
