#include "server.cpp"
#include "client.cpp"

/*
 * 
-Filesize (256b, 1kb, 4kb, 32, 256, 1mb, 4mb, 32mb?)
-Number of worker threads (1, 2, 4, 8, 16, 32?)
-Queue size (1, 2, 4, 8, 16, 32)
-Number of clients (1, 2, 4, 8, 16)
-loops? (1, 2, 4, 8)
*/

int workerThreadsNum = 1;
int workerThreads[] = {1, 2, 4, 8, 16};
int queueSizeNum = 1;
int queueSize[] = {1, 2, 4, 8, 16, 32};
int clientThreadsNum = 1;
int clientThreads[] = {1, 2, 4, 8, 16};
int fileSizeNum = 1;
char* fileSize[] = {"256b", "1kb", "4kb", "32kb", "256kb", "1mb", "4mb"};
int loopSizeNum = 3;
int loopSize[] = {1, 2, 4, 8};

int main(int argc, char* argv[])
{
	//Server loops
	
	int port = 2000;
	
	//Worker threads
	for(int i = 0; i < workerThreadsNum; i++)
	{
		//for(int j = 0; j < queueSizeNum; j++)
		{
			HTTP_Server srv(++port);
			srv.setupThreadPool(workerThreads[i]);
			srv.beginAcceptLoop();
			
			//wait for the startup
			sleep(1);
			
			//Worker loop
			for(int k = 0; k < clientThreadsNum; k++)
			{
				for(int l = 0; l < fileSizeNum; l++)
				{
					for(int m = 0; m < loopSizeNum; m++)
					{
						//cout << "Client" << endl;
						client clnt(port);
						clnt.runWorkerThreads(clientThreads[k], loopSize[m]);
					}
				}
			}
			
			//Wait 5 before starting a new server with a new port
			sleep(5);
		}
	}
}
