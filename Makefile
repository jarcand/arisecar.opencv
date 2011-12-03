default: client server

server:
	gcc stream_server.c `pkg-config --cflags opencv` `pkg-config --libs opencv` -lpthread -o stream_server.bin

client:
	gcc stream_client.c `pkg-config --cflags opencv` `pkg-config --libs opencv` -lpthread -o stream_client.bin
