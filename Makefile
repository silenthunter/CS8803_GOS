default: server.cpp
	g++ -g -o http_server server_driver.cpp server.cpp -lpthread 
	g++ -g -o http_client client_driver.cpp client.cpp -lpthread 
	g++ -g -o http_proxy proxy_driver.cpp server.cpp proxy.cpp -lpthread -DUSESHARED
	g++ -g -o http_proxy_noShm proxy_driver.cpp server.cpp proxy.cpp -lpthread

tests: UnitTests.cpp
	g++ -o UnitTests UnitTests.cpp -I../gtest-1.6.0/include/ -L../gtest-1.6.0/ -lpthread -lgtest -std=c++0x

run-tests: tests
	./UnitTests

analyze: 
	g++ -g -o testSuite testSuite.cpp server.cpp client.cpp -lpthread 

analyzeSocket: 
	g++ -g -o testSuiteNoShm testSuite_noshm.cpp server.cpp client.cpp -lpthread 

clean:
	rm -f *.out UnitTests http_server http_client http_proxy *.o testSuite http_proxy_noShm
