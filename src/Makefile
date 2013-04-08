#
# Makefile for BoostStomp
# Elias Karakoulakis <elias.karakoulakis@gmail.com>

# GNU make only

.SUFFIXES:	.cpp .o .a .s

# Sorry gcc, clang is way better.
# Use it if found in path...
CLANG := $(shell which clang)
ifeq ($(CLANG),)
CC     := gcc
CXX    := g++
else
CC     := clang
CXX    := clang++
endif

LD     := ld
AR     := ar rc
RANLIB := ranlib

# TARGET may be DEBUG or RELEASE
ifeq ($(TARGET),"")
	TARGET := DEBUG
endif


DEBUG_CFLAGS    := -Wno-format -g -DDEBUG -Werror -O0 -DDEBUG_STOMP -DBOOST_ASIO_ENABLE_BUFFER_DEBUGGING
RELEASE_CFLAGS  := -Wall -Wno-unknown-pragmas -Wno-format -O3 -DNDEBUG
CFLAGS	:= -c $($(TARGET)_CFLAGS) -fPIC

DEBUG_LDFLAGS	:= -g
RELEASE_LDFLAGS := 
LDFLAGS	:= $($(TARGET)_LDFLAGS) -lboost_system -lboost_thread

INCLUDES := -I .
DESTDIR := /usr/local
VERSION := 1.0

%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

all: main static_lib shared_lib
        	
main:   Main.o  BoostStomp.o StompFrame.o helpers.o
	$(CXX) -o $@ Main.o BoostStomp.o StompFrame.o helpers.o $(LDFLAGS)
#	upx main
	
static_lib:	BoostStomp.o StompFrame.o
	$(AR) libbooststomp.a BoostStomp.o StompFrame.o helpers.o
ifeq ('$(TARGET)','RELEASE')
	strip libbooststomp.a
endif
	
shared_lib:
	$(CXX) -o libbooststomp.so.$(VERSION) BoostStomp.o StompFrame.o helpers.o \
	-shared -Wl,-soname,libbooststomp.so.$(VERSION) $(LDFLAGS)
ifeq ('$(TARGET)','RELEASE')
	strip libbooststomp.so*
endif
