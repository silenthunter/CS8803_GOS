#include "server.cpp"
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

TEST(bossThreadTest, SingleConnection)
{
	HTTP_Server srv(25000);
	srv.beginAcceptLoop();
	
	//Wait for the server to bind and listen
	sleep(2);
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	hostent *server = gethostbyname("127.0.0.1");
	
	struct sockaddr_in serv_addr;
	
	ASSERT_NE(server, (hostent*)0x0);
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);
	serv_addr.sin_port = htons(25000);
	
	int connected = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	EXPECT_EQ(connected, 0);
	
}

int main(int argc, char* argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
