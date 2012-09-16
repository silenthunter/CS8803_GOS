default: server.cpp
	g++ -g -o http_server server_driver.cpp server.cpp -lpthread

tests: UnitTests.cpp
	g++ -o UnitTests UnitTests.cpp -I../gtest-1.6.0/include/ -L../gtest-1.6.0/ -lpthread -lgtest

run-tests: tests
	./UnitTests

clean:
	rm -f *.out UnitTests http_server *.o
