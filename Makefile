CC = gcc
CFLAGS = -std=c11
TARGET = proxy_server
OBJECTS = err_hdl.o proxy.o

all : $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean :
		rm *.o proxy_server
