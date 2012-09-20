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
};
