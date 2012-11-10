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
#define SHSIZE 4096 //The size of each segment

#define SENDSIZE 2048//The bytes read from the file during each iteration

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
	
	///Pthreads condition that will broadcast when a thread finishes using a section of shared memory
	pthread_cond_t bufferCondition;
	
	///The mutex that will lock to change information about the shared memory queue
	pthread_mutex_t bufferLock;
	
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
	
	///The number of threads currently using shared memory
	int shMemThreads;

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
	enum {FREE, LOCKED, MODIFIED, READ};
	
	///Flags that represent the different return methods for data going to the client
	typedef enum {GET, SHBUFF} DataMethod;

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
	
	/**
	 * @brief Gains ownership of a section of shared memory
	 * @return The index of the shared memory section in shMem
	 * 
	 * @note Blocks until shared memory can be acquired
	 */
	int acquireSharedMem()
	{
		int retn;
		
		pthread_mutex_lock(&bufferLock);
		sharedQueue[queueTail] = (unsigned int)pthread_self();
		queueTail = (queueTail + 1) % SHNUM;
		
		while(shMemThreads >= SHNUM || sharedQueue[queueHead] != (unsigned int)pthread_self())
			pthread_cond_wait(&bufferCondition, &bufferLock);
			
		queueHead = (queueHead + 1) % SHNUM;
		shMemThreads++;
		
		pthread_mutex_unlock(&bufferLock);
		
		//Find a free shmem segment
		for(int i = 0; i < SHNUM; i++)
			if(*(shMem[i]) == FREE)
			{
				retn = i;
				*(shMem[i]) = LOCKED;
				break;
			}
		
		pthread_mutex_lock(&bufferLock);
		pthread_cond_broadcast(&bufferCondition);
		pthread_mutex_unlock(&bufferLock);
		
		return retn;
	}
	
	void releaseSharedMem(int shId)
	{
		pthread_mutex_lock(&bufferLock);
		
		*(shMem[shId]) = FREE;
		
		shMemThreads--;
		
		pthread_cond_broadcast(&bufferCondition);
		pthread_mutex_unlock(&bufferLock);
	}
	
	/**
	 * @brief Detaches from shared memory
	 */
	void cleanupSharedMem()
	{
		for(int i = 0; i < SHNUM; i++)
			shmdt(shMem[i]);
		
		delete sharedQueue;
		delete shMem;
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
		
		pthread_mutex_init(&bufferLock, NULL);
		pthread_cond_init(&bufferCondition, NULL);
		
		pthread_attr_init(&attr);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		
		rootDir = "WWW/";
		
		queueTail = 0;
		queueHead = 0;
		
		shMemThreads = 0;
		
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
			
			cout << "Method: " << method << endl << "File: " << file << endl;
			
			DataMethod methodFlag = GET;
			if(method.compare("SHBUFF") == 0) methodFlag = SHBUFF;
			
			parseHTTPRequest(file, data->socketNum, methodFlag);
			//cout << fileContents << endl;
			
			close(data->socketNum);
			
			delete data;
		}
	}
	
	/**
	 * @brief Sends data to the client using the method passed in
	 */
	int sendData(int retnId, const char* data, int len, DataMethod method)
	{
		//Use the shared memory buffer
		if(method == SHBUFF)
		{
			while(*(shMem[retnId]) == MODIFIED);//Spin while the client reads the data
			
			pthread_mutex_lock((pthread_mutex_t*)(shMem[retnId] + 1));
			
			char* mutexEnd = (char*)(shMem[retnId] + 1) + sizeof(pthread_mutex_t);
			char* dataStart = mutexEnd + sizeof(int);
			
			bcopy(data, dataStart, len);
			bcopy(&len, mutexEnd, sizeof(int));
			
			*(shMem[retnId]) = MODIFIED;
			
			pthread_mutex_unlock((pthread_mutex_t*)(shMem[retnId] + 1));
			
			return 0;
		}
		//Use sockets
		else
		{
			return write(retnId, data, len);
		}
	}
	
	/**
	 * @brief Retrieves the file and returns the string to send back to the client
	 * 
	 * @param fileName The name of the file to be retrieved
	 * @param socketNum The socket number of the client that requested this file
	 * @param The method that will be used to send the data back
	 * 
	 */
	virtual void parseHTTPRequest(string fileName, int socketNum, DataMethod method)
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
		
		//send the client the shared memory ID
		if(method == SHBUFF)
		{
			int shMemID = acquireSharedMem();
			write(socketNum, &shMemID, sizeof(int));
			
			socketNum = shMemID;
		}
		
		//Make sure they stay withing 
		if(fileName.find("..") != string::npos)
		{
			header = "HTTP/1.0 403 Forbidden";
			body = "403 Forbidden";
			
			string combined = header + "\n\n" + body;
			sendData(socketNum, combined.c_str(), strlen(combined.c_str()), method);
		}
		//Make sure the file opened
		else if(inFile.is_open())
		{
			header = "HTTP/1.0 200 OK\n\n";
			
			//Write the header first
			sendData(socketNum, header.c_str(), strlen(header.c_str()), method);
			
			char strBuf[SENDSIZE];
			inFile.seekg(0, ios::end);
			int length = inFile.tellg();
			inFile.seekg(0, ios::beg);
			while(length > 0)
			{
				inFile.read(strBuf, min(SENDSIZE, length));
				int err = sendData(socketNum, strBuf, min(SENDSIZE, length), method);
				length -= SENDSIZE;
			}
			inFile.close();
		}
		else
		{
			header = "HTTP/1.0 404 Not Found";
			body = "Page not found";
			
			string combined = header + "\n\n" + body;
			sendData(socketNum, combined.c_str(), strlen(combined.c_str()), method);
			
			//I need to make sure this doesn't happen in benchmarking
			cout << "404!" << endl;
		}
		
		//Release the shared memory
		if(method == SHBUFF)
		{
			sendData(socketNum, NULL, 0, method);
			while(*(shMem[socketNum]) != READ);//Spin while the client reads the final segment
			
			//Acquire the lock to ensure that the client is done
			pthread_mutex_lock((pthread_mutex_t*)(shMem[socketNum] + 1));
			pthread_mutex_unlock((pthread_mutex_t*)(shMem[socketNum] + 1));
			releaseSharedMem(socketNum);
		}
	}
};
