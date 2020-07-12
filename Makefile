CC = gcc
CFLAGS = -std=c11
TARGET = server
OBJECTS = err_hdl.o server.o

all : $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean :
		rm *.o server
