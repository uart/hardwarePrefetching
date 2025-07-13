# Console Project Makefile
# Location: tools/console/Makefile

# Define source files
UI_SRCS = ui/console.c ui/console_views.c
SRC_SRCS = src/metrics.c src/snapshot.c src/sysinfo.c src/tuning.c
ROOT_SRCS = ../../user_api.c ../../pcie.c ../../pmu_ddr.c ../../log.c ../../sysdetect.c

# Combine all sources into one variable
SOURCES = $(UI_SRCS) $(SRC_SRCS) $(ROOT_SRCS)

# Basic flags and target
CFLAGS = -Wall -O2 -g
INCLUDES = -Iinclude -I../../include -I../../kernelmod
LDFLAGS = -lncurses -lpci
TARGET = console

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)
