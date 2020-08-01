CC = gcc
CFLAGS = -pthread

TARGET_PROXY = proxy_server
OBJECTS_PROYX = 		\
	proxy.o 			\
	handler/error.o		\
	handler/atomic.o	\
	handler/socket.o 	\

TARGET_CLIENT = client
OBJECTS_CLIENT = 		\
	client.o			\
	handler/error.o		\

TARGET_SERVER = web_server
OBJECTS_SERVER = 		\
	server.o			\
	handler/error.o		\

all : $(TARGET_PROXY)

$(TARGET_PROXY) : $(OBJECTS_PROYX)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm $(TARGET_SERVER) $(OBJECTS_PROYX)