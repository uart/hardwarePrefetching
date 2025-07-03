# ----------------------------------------
# Makefile for dPF Console (UI + Backend)
# ----------------------------------------

CC := gcc
CFLAGS := -Wall -O2 -g
INCLUDES := -Iinclude -I../../../include
LIBS := -lncurses

# UI source files
UI_SRC := $(wildcard *.c)

# Backend and shared modules
BACKEND_SRC := \
    ../src/metrics.c \
    ../src/snapshot.c \
    ../../../user_api.c \
    ../../../sysdetect.c \
    ../../../log.c

# Final executable
TARGET := console

all: $(TARGET)

$(TARGET): $(UI_SRC) $(BACKEND_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(UI_SRC) $(BACKEND_SRC) $(LIBS)

clean:
	rm -f $(TARGET)
	