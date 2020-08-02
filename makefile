# =================================================
CC = gcc
CFLAGS = -pthread
# =================================================
TARGET_PROXY = proxy_server
OBJECTS_PROYX = 		\
	proxy.o 			\
	handler/error.o		\
	handler/atomic.o	\
	handler/socket.o 	\
	handler/signal.o	\
# =================================================
TARGET_CLIENT = client
OBJECTS_CLIENT = 		\
	client.o			\
	handler/error.o		\
	handler/socket.o	\
# =================================================
TARGET_SERVER = web_server
OBJECTS_SERVER = 		\
	server.o			\
	handler/error.o		\
# =================================================
all : $(TARGET_PROXY) $(TARGET_CLIENT)

$(TARGET_PROXY) : $(OBJECTS_PROYX)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_CLIENT) : $(OBJECTS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_SERVER) : $(OBJECTS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm	$(TARGET_PROXY) 	$(TARGET_CLIENT)	\
		*.o		handler/*.o
# =================================================