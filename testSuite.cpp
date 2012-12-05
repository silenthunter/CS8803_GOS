#include "server.cpp"
#include "proxy.cpp"
#include "client.cpp"

/*
 * 
-Filesize (256b, 1kb, 4kb, 32, 256, 1mb, 4mb, 32mb?)
-Number of worker threads (1, 2, 4, 8, 16, 32?)
-Queue size (1, 2, 4, 8, 16, 32)
-Number of clients (1, 2, 4, 8, 16)
-loops? (1, 2, 4, 8)
*/

int workerThreadsNum = 5;
int workerThreads[] = {1, 2, 4, 8, 16};
int queueSizeNum = 1;
int queueSize[] = {32, 2, 4, 8, 16, 32};
int clientThreadsNum = 5;
int clientThreads[] = {1, 2, 4, 8, 16};
int fileSizeNum = 4;
char* fileSize[] = {"263kb.jpg", "1mb.jpg", "4mb.jpg", "34_2mb.jpg"};
int loopSizeNum = 1;
int loopSize[] = {1, 2, 4, 8};

int main(int argc, char* argv[])
{
	//Server loops
	
	int port = 30000;
	
	//Worker threads
	for(int i = 4; i < workerThreadsNum; i++)
	{
		for(int j = 0; j < queueSizeNum; j++)
		{
			HTTP_Server srv(++port + 1000, queueSize[j]);
			srv.setupThreadPool(workerThreads[i]);
			srv.beginAcceptLoop();
			
			sleep(1);
			
			HTTP_Proxy prx(port, queueSize[j], port + 1000);
			prx.setupThreadPool(workerThreads[i]);
			prx.beginAcceptLoop();
			
			//wait for the startup
			sleep(1);
			
			//Worker loop
			for(int k = 0; k < clientThreadsNum; k++)
			{
				for(int l = 0; l < fileSizeNum; l++)
				{
					for(int m = 0; m < loopSizeNum; m++)
					{
						sleep(1);
						printf("# %d %d %d %d %d\n", i, j, k, l, m);
						
						//Convert the decimal digit to ascii
						//char num = rand() % 9 + 48;
						client clnt(port);
						char randName[128];
						sprintf(randName, "%s%d", fileSize[l], rand() % 10);
						clnt.runWorkerThreads("localhost", randName, clientThreads[k], loopSize[m], NULL);
					}
				}
			}
			
			srv.shutdownServer();
			//Wait 5 before starting a new server with a new port
			sleep(5);
		}
	}
}
