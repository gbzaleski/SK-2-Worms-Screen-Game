CXXSOURCES_SERVER = server_main.cpp UDP_server.cpp UDP_server.h randomiser.cpp randomiser.h game.cpp game.h game_constant.h
CXXSOURCES_CLIENT = client_main.cpp game_constant.h
CXX = g++
CXXFLAGS = -pthread -Wall -Wextra -Werror

all: server client

server: 
	$(CXX) $(CXXSOURCES_SERVER) $(CXXFLAGS) -o screen-worms-server
	
client:
	$(CXX) $(CXXSOURCES_CLIENT) $(CXXFLAGS) -o screen-worms-client

.PHONY: clean
clean:
	rm -rf *.o screen-worms-server screen-worms-client
