struct workerThreadStruct
{
	int loopLimit;
};

/**
 * @brief Allows for connection to and requesting pages from an HTTP Server
 */
class client
{
	private:
	///The port the remote server listens on
	int port;
	
	public:
	
	/**
	 * @brief Basic constructor that will initialize the HTTP client
	 * 
	 * @param remotePort The port the remote server listens on
	 */
	client(int remotePort)
	{
		port = remotePort;
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
	}
	
	/**
	 * @brief The worker thread that will actually connect
	 *  and write to the HTTP server
	 * 
	 * @param dataStruct The data structure that contains loop
	 * and connection information
	 */
	void* workerThread(void* dataStruct)
	{
		workerThreadStruct* data = (workerThreadStruct*) dataStruct;
	}
};
