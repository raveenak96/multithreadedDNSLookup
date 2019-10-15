CC = gcc
CFLAGS = -Wall -Wextra
TARGET = multi-lookup

all: $(TARGET)

$(TARGET): $(TARGET).c queue.c util.c
	$(CC) $(CFLAGS) -pthread -o $(TARGET) $(TARGET).c queue.c util.c
	find . -type d -print0 | xargs -0 chmod 777
clean: 
	$(RM) $(TARGET) *.o
