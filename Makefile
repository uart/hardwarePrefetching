CFLAGS = -Wall -Wextra -O2 -g -Iinclude/ -I/usr/include/cjson -pthread
LDFLAGS = -lm -lcjson
TARGET = dpf

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c pmu.c rdt_mbm.c sysdetect.c tuners/primitive.c tuners/mab.c tuners/mab_setup.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c msr.c pmu.c rdt_mbm.c sysdetect.c tuners/primitive.c tuners/mab.c tuners/mab_setup.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
