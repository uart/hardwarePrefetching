CFLAGS = -Wall -Wextra -O2 -g -Iinclude/ -I/usr/include/cjson -pthread
LDFLAGS = -lm -lcjson -lpci
TARGET = dpf

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c log.c msr.c pmu_core.c pmu_ddr.c rdt_mbm.c sysdetect.c pcie.c tuners/primitive.c tuners/mab.c tuners/mab_setup.c json_parser.c user_api.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c log.c msr.c pmu_core.c pmu_ddr.c rdt_mbm.c sysdetect.c pcie.c tuners/primitive.c tuners/mab.c tuners/mab_setup.c json_parser.c user_api.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
