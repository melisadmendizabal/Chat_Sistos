CC = gcc
CFLAGS = -Wall -pthread
TARGET = servidor

all: $(TARGET)

$(TARGET): servidor.c
	$(CC) $(CFLAGS) -o $(TARGET) servidor.c

clean:
	rm -f $(TARGET)