/**
 * @file server.cpp
 * @author Gavin Gresham <gavin.gresham@cs.gatech.edu>
 * 
 * @section DESCRIPTION
 * Contains the HTTP_Server class
 */

/**
 * @brief A multi-threaded server that parses basic HTTP commands
 */
class HTTP_Server
{
	private:
	int port;

	public:

	/**
	 * @brief Instantiates the HTTP Server object
	 * 
	 * @param port The port the server will bind to
	 */
	void HTTP_Server(int port)
	{
		this.port = port;
	}

	/**
	 * Initializes a worker thread pool that will handle the client requests
	 * 
	 * @param poolSize The size of the threadpool that will be initialized
	 */
	void setupThreadPool(int poolSize)
	{
	}

	/**
	 * @brief Starts the boss thread that will accept incoming
	 *  connections and pass them to worker threads.
	 * 
	 * @note Uses the port defined in @ref HTTP_Server(int port)
	 */
	void beginAcceptLoop()
	{
	}
	
	/**
	 * Retrieves the file and sends it back to the client
	 * 
	 * @param fileName The name of the file to be retrieved
	 * @param sockNo The file descriptor for the client's socket
	 */
	void parseHTTPRequest(string fileName, int sockNo)
	{
	}
}
