CFLAGS = -Wall -Wextra -O2 -g -I/usr/include/cjson
LDFLAGS = -lm -lcjson
TARGET = dpf

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c pmu.c mab.c mab_setup.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c msr.c pmu.c mab.c mab_setup.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
