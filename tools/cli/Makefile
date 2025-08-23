CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I$(CURDIR)/../../include -I$(CURDIR)/../../kernelmod
LDFLAGS = -lm

# Source files
SRCS = dpfctrl.c ../../user_api.c ../../log.c

# Target binary
TARGET = dpfctrl

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
