default: server.cpp
	g++ -g -o http_server server_driver.cpp server.cpp -lpthread -std=c++0x
	g++ -g -o http_client client_driver.cpp client.cpp -lpthread -std=c++0x

tests: UnitTests.cpp
	g++ -o UnitTests UnitTests.cpp -I../gtest-1.6.0/include/ -L../gtest-1.6.0/ -lpthread -lgtest -std=c++0x

run-tests: tests
	./UnitTests

clean:
	rm -f *.out UnitTests http_server http_client *.o
