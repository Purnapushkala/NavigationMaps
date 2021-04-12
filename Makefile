all: server client

server: server.o dijkstra.o digraph.o
	g++ -o server/server server/server.o server/dijkstra.o server/digraph.o

server.o: server/server.cpp 
	g++ -c server/server.cpp -o server/server.o

dijkstra.o: server/dijkstra.cpp server/dijkstra.h server/heap.h
	g++ -c server/dijkstra.cpp -o server/dijkstra.o

digraph.o: server/digraph.cpp server/digraph.h server/wdigraph.h
	g++ -c server/digraph.cpp -o server/digraph.o

client: client.o
	g++ -o client/client client/client.o

client.o: client/client.cpp
	g++ -c client/client.cpp -o client/client.o

clean: 
	rm -f inpipe outpipe server/server.o server/digraph.o server/dijkstra.o client/client.o