CC = gcc

CFLAGS = -std=c11 -pthread

TARGET1 = proxy_server.out
OBJECTS1 = proxy.o err_hdl.o queue.o

TARGET2 = client.out
OBJECTS2 = client.o err_hdl.o

all : $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJECTS1)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET2): $(OBJECTS2)
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm *.o proxy_server.out client.out
