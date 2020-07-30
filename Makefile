CC = gcc
CFLAGS = -pthread

TARGET1 = proxy.out
OBJECTS1 = proxy.o err_hdl.o

TARGET2 = client.out
OBJECTS2 = client.o err_hdl.o

all : $(TARGET2) $(TARGET1)

$(TARGET2): $(OBJECTS2)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET1): $(OBJECTS1)
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm *.o *.out