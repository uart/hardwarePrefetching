CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
TARGET = dpf

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c pmu.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c msr.c pmu.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
