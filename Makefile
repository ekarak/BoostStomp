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
        
helpers.o:   helpers.cpp  helpers.h
	$(CXX) $(CFLAGS) -c helpers.cpp $(INCLUDES)
	
BoostStomp.o:  BoostStomp.cpp BoostStomp.hpp
	$(CXX) $(CFLAGS) -c BoostStomp.cpp $(INCLUDES)
	
StompFrame.o:  StompFrame.cpp StompFrame.hpp
	$(CXX) $(CFLAGS) -c StompFrame.cpp $(INCLUDES)

Main.o: Main.cpp 
	$(CXX) $(CFLAGS) -c Main.cpp $(INCLUDES)  
	
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

install: static_lib shared_lib
	install -d $(DESTDIR)/include/booststomp
	install -d $(DESTDIR)/lib
	install libbooststomp.so.$(VERSION) $(DESTDIR)/lib
	ln -sf  $(DESTDIR)/lib/libbooststomp.so.$(VERSION) $(DESTDIR)/lib/libbooststomp.so 
	install libbooststomp.a $(DESTDIR)/lib
	ldconfig $(DESTDIR)/lib
	cp -r *.h   $(DESTDIR)/include/booststomp
	cp -r *.hpp $(DESTDIR)/include/booststomp
	
uninstall:
	rm -rf $(DESTDIR)/include/booststomp
	rm -f $(DESTDIR)/lib/libbooststomp*

dist:	main
	rm -f BoostStomp.tar.gz
	tar -c --exclude=".git" --exclude ".svn" --exclude "*.o" -hvzf BoostStomp.tar.gz *.cpp *.h *.hpp Makefile license/ README*

bindist: main
	rm -f BoostStomp_bin_`uname -i`.tar.gz
	tar -c --exclude=".git" --exclude ".svn" -hvzf BoostStomp_bin_`uname -i`.tar.gz main *.a *.so license/ README*

clean:
	rm -f main *.o *.a libbooststomp.so*
