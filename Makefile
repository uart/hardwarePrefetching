CFLAGS = -Wall -Wextra -O2 -g -I/usr/include/cjson -pthread
LDFLAGS = -lm -lcjson
TARGET = dpf

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c pmu.c mab.c mab_setup.c rdt_mbm.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c msr.c pmu.c mab.c mab_setup.c rdt_mbm.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
