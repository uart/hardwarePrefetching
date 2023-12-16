CFLAGS = -Wall -Wextra -O2
TARGET = main 

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c

clean:
	rm -f $(TARGET)
