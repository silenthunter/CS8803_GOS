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
#include <fstream>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define SHMEM
#define SHNUM 6 //The number of shared memory segments
#define SHSIZE 1024 //The size of each segment

using namespace std;

struct requestData
{
	int socketNum;
};

/**
 * @brief A multi-threaded server that parses basic HTTP commands
 */
class HTTP_Server
{
	protected:
	///The port that the server listens on
	int port;
	
	///The number of clients that can connect before a worker starts processing a client
	int queueSize;
	
	///Array of worker threads that will handle client requests
	pthread_t* workerThreads;
	
	///The number of worker threads
	int workerThreadCount;
	
	///The "boss" thread that accepts all incoming connections
	pthread_t masterThread;
	
	///The attributes threads run with
	pthread_attr_t attr;
	
	///Pthreads condition that will be 'signaled' when a client is accepted
	pthread_cond_t acceptCondition;
	
	///Controls whether the server is running or not
	bool running;
	
	///The mutex that synchronizes the boss thread and the worker threads
	pthread_mutex_t acceptLock;
	
	///Queue that file requests are placed in
	queue<requestData*> requestQueue;

	///The socket that is handling accepts
	int bossfd;
	
	///The path of the documents folder that the server will read from
	const char* rootDir;

	///The current head of the shared memory queue
	unsigned int queueHead;

	///The current tail of the shared memory queue
	unsigned int queueTail;

	///Queue for getting a shared memory slot
	int* sharedQueue;
	
	///An array of pointers to the sections of shared memory
	int** shMem;
	
	//Boolean that tracks the running state of the server
	//static bool serverRunning;
	
	static void error(string errorText)
	{
		cout << errorText << endl;
	}
	
	/**
	 * @brief Flags that represents the current state of a shared memory slot
	 */
	enum {FREE, LOCKED, MODIFIED, READ} SharedState;

	/**
	* @brief Creates and connects to shared memory, and if this is the first process to attach, initialize the state and mutex
	* 
	* @note 4 Byte state, Shared Mutex, 4 Byte data length, data
	*/
	void setupSharedMem()
	{
		sharedQueue = new int[workerThreadCount];
		shMem = new int* [SHNUM];
		
		for(int i = 0; i < SHNUM; i++)
		{
			int sharedID = 0;
			key_t sharedKey = 7890 + i;

			//create shared memory
			if((sharedID = shmget(sharedKey, SHSIZE, IPC_CREAT | 0666)) < 0)
				cout << "Error created shared memory" << endl;
			
			//Get shared memory info
			shmid_ds stats;	
			if(shmctl(sharedID, IPC_STAT, &stats) < 0)
				cout << "Error in shared memory Stat" << endl;
				
			//Attach to the memory
			if((shMem[i] = (int*)shmat(sharedID, 0, 0)) == (int*)-1)
				cout << "Error attaching to shared memory" << endl;	
				
			//If this is the first to attach, initialize the mutex and state
			if(stats.shm_nattch == 0)
			{
				//Init state
				*(shMem[i]) = FREE;
				//Create the shared mutex
				pthread_mutex_init((pthread_mutex_t*)shMem[i] + 1, NULL);
			}		
		}
	}
	

	public:

	/**
	 * @brief Instantiates the HTTP Server object
	 * 
	 * @param port The port the server will bind to
	 * @param acceptQueueSize The number of clients to keep in a queue before additional connections are rejected
	 */
	HTTP_Server(int serverPort, int acceptQueueSize)
	{
		port = serverPort;
		queueSize = acceptQueueSize;
		running = true;
		
		pthread_mutex_init(&acceptLock, NULL);
		pthread_cond_init(&acceptCondition, NULL);
		
		pthread_attr_init(&attr);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		
		rootDir = "WWW/";
		
#ifdef SHMEM
		setupSharedMem();
#endif
	}

	/**
	 * @brief Initializes a worker thread pool that will handle the client requests
	 * 
	 * @param poolSize The size of the threadpool that will be initialized
	 */
	void setupThreadPool(int poolSize)
	{
		cout << "Setting up" << endl;
		workerThreadCount = poolSize;
		workerThreads = new pthread_t[poolSize];
		for(int i = 0; i < poolSize; i++)
		{
			workerThreads[i] = pthread_t();
			pthread_create(&workerThreads[i], &attr, HTTP_Server::launchWorkerThreadTask, this);
		}
	}
	
	/**
	 * @brief Stops all worker threads and the boss thread
	 */
	void shutdownServer()
	{
		pthread_cancel(masterThread);
		pthread_join(masterThread, NULL);
		
		pthread_mutex_lock(&acceptLock);
		
		running = false;
		pthread_cond_broadcast(&acceptCondition);
		
		pthread_mutex_unlock(&acceptLock);
		
		for(int i = 0; i < workerThreadCount; i++)
		{
			//pthread_cancel(workerThreads[i]);
			pthread_join(workerThreads[i], NULL);
		}
	}

	/**
	 * @brief Starts the boss thread that will accept incoming
	 *  connections and pass them to worker threads.
	 * 
	 * @note Uses the port defined in the constructor
	 */
	void beginAcceptLoop()
	{
		pthread_create(&masterThread, &attr, HTTP_Server::launchBossThread, this);
	}
	
	/**
	 * @brief A level of indirection to call the member function bossThread for a thread
	 * 
	 * @param obj The object that is creating the thread
	 */
	static void *launchBossThread(void* obj)
	{
		HTTP_Server* thisSrv = static_cast<HTTP_Server*>(obj);
		thisSrv->bossThread(thisSrv);
	}
	
	/**
	 * @brief The boss thread that will loop while accepting client 
	 * connections and handing them to worker threads
	 * 
	 * @note http://www.linuxhowtos.org/C_C++/socket.htm
	 */
	virtual void *bossThread(void* input)
	{
		HTTP_Server* thisSrv = (HTTP_Server*) input;
		int portIN = thisSrv->port;
		struct sockaddr_in serv_addr;
		
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		thisSrv->bossfd = sockfd;
		if(sockfd < 0)
		{
			error("Unable to open socket");
			printf("errno: %d (%d, %d, %d)\n", errno, EBADF, EINTR, EIO);
		}
			
		//Clear the structs
		bzero(&serv_addr, sizeof(serv_addr));
		
		//sets server information
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(portIN);//Host to network
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		
		if(bind(sockfd, (const sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
			error("Bind failed on port: " + portIN);
			
		listen(sockfd, 5);
		
		//Loop and accept connections
		while(thisSrv->running)
		{
			struct sockaddr_in *client_addr = new sockaddr_in;
			bzero(&client_addr, sizeof(client_addr));
			socklen_t clientlen = sizeof(*client_addr);
			int newsockfd = accept(sockfd, (struct sockaddr *) client_addr, &clientlen);
			
			//Should skip the interrupted accept
			if(newsockfd <= 0) continue;
			
			pthread_mutex_lock(&thisSrv->acceptLock);
			
			
			if(thisSrv->requestQueue.size() >= thisSrv->queueSize)
			{
				//cout << "Full!" << endl;
				close(newsockfd);
			}
			else
			{				
				//Add this connection to the request queue
				struct requestData* data = new requestData();
				data->socketNum = newsockfd;
				thisSrv->requestQueue.push(data);
				//cout << "Size: " << thisSrv->requestQueue.size() << endl;
				
				//Signal waiting workers
				pthread_cond_signal(&thisSrv->acceptCondition);
			}
			
			pthread_mutex_unlock(&thisSrv->acceptLock);
		}
		
		//pthread_exit(0);
	}
	
	/**
	 * @brief A level of indirection to call the member function workerThreadTask for a thread
	 * 
	 * @param obj The object that is creating the thread
	 */
	static void *launchWorkerThreadTask(void* obj)
	{
		HTTP_Server* thisSrv = static_cast<HTTP_Server*>(obj);
		thisSrv->workerThreadTask(thisSrv);
	}
	
	/**
	 * @brief The method worker tasks run that receive client information
	 * from the boss thread
	 */
	virtual void *workerThreadTask(void* input)
	{
		HTTP_Server* thisSrv = (HTTP_Server*)input;
		char buffer[255];
		
		while(thisSrv->running)
		{
			requestData *data;
			
			#pragma region Critical Section
			pthread_mutex_lock(&thisSrv->acceptLock);
			if(!thisSrv->running)
			{
				pthread_mutex_unlock(&thisSrv->acceptLock);
				return NULL;
			}
			
			//Loop while there aren't any requests
			while(thisSrv->requestQueue.size() == 0)
			{
				pthread_cond_wait( &thisSrv->acceptCondition, &thisSrv->acceptLock );
				if(!thisSrv->running)
				{
					pthread_mutex_unlock(&thisSrv->acceptLock);
					return NULL;
				}
			}
			
			//Get the next client in the queue
			data = thisSrv->requestQueue.front();
			thisSrv->requestQueue.pop();
			
			pthread_mutex_unlock(&thisSrv->acceptLock);
			#pragma endregion
			
			string input = "";
			
			bool EOL_Found = false;
			int bytesRead = 1;
			while(!EOL_Found)
			{
				bytesRead = read(data->socketNum, &buffer, 255);
				input += string(buffer, 0, bytesRead);
				
				//Carriage returns exists in the protocol
				if(input.find("\r\n\r\n") != string::npos)
					EOL_Found = true;
			}
			
			if(bytesRead < 0) continue;
			
			//cout << input << endl;
			
			int idx1 = input.find(' ');
			int idx2 = input.find(' ', idx1 + 1);
			
			//TODO: Catch malformed requests
			
			string method = input.substr(0, idx1);
			string file = input.substr(idx1 + 1, idx2 - idx1 - 1);
			
			//cout << "Method: " << method << endl << "File: " << file << endl;
			
			parseHTTPRequest(file, data->socketNum);
			//cout << fileContents << endl;
			
			close(data->socketNum);
			
			delete data;
		}
	}
	
	/**
	 * @brief Retrieves the file and returns the string to send back to the client
	 * 
	 * @param fileName The name of the file to be retrieved
	 * @param socketNum The socket number of the client that requested this file
	 * 
	 */
	virtual void parseHTTPRequest(string fileName, int socketNum)
	{
		//Prepend the document root
		fileName = rootDir + fileName;
		
		string retn, tmpLine;
		string header, timeStr, body;
		ifstream inFile(fileName.c_str());
		
		//Get the time
		time_t rawtime;
		struct tm* timeInfo;
		
		time(&rawtime);
		timeInfo = gmtime(&rawtime);
		timeStr = asctime(timeInfo);
		
		//Make sure they stay withing 
		if(fileName.find("..") != string::npos)
		{
			header = "HTTP/1.0 403 Forbidden";
			body = "403 Forbidden";
			
			string combined = header + "\n\n" + body;
			write(socketNum, combined.c_str(), strlen(combined.c_str()));
		}
		//Make sure the file opened
		else if(inFile.is_open())
		{
			header = "HTTP/1.0 200 OK\n\n";
			
			//Write the header first
			write(socketNum, header.c_str(), strlen(header.c_str()));
			
			char strBuf[1024];
			inFile.seekg(0, ios::end);
			int length = inFile.tellg();
			inFile.seekg(0, ios::beg);
			while(length > 0)
			{
				inFile.read(strBuf, min(1024, length));
				int err = write(socketNum, strBuf, min(1024, length));
				length -= 1024;
			}
			inFile.close();
		}
		else
		{
			header = "HTTP/1.0 404 Not Found";
			body = "Page not found";
			
			string combined = header + "\n\n" + body;
			write(socketNum, combined.c_str(), strlen(combined.c_str()));
			
			//I need to make sure this doesn't happen in benchmarking
			cout << "404!" << endl;
		}
		
		//Combine the HTTP components
		retn = header + "\n" + timeStr + "\n" + body;
	}
};
