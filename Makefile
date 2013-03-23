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

# Change for DEBUG or RELEASE
TARGET := DEBUG

DEBUG_CFLAGS    := -Wall -Wno-format -g -DDEBUG -Werror -O0 -DDEBUG_STOMP -DBOOST_ASIO_ENABLE_BUFFER_DEBUGGING
RELEASE_CFLAGS  := -Wall -Wno-unknown-pragmas -Wno-format -O3 -DNDEBUG

DEBUG_LDFLAGS	:= -g

CFLAGS	:= -c $($(TARGET)_CFLAGS) -fPIC
LDFLAGS	:= $($(TARGET)_LDFLAGS) -L/usr/lib/ -L/usr/local/lib -lboost_system -lboost_thread
INCLUDES := -I .

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
	
shared_lib:
	$(CXX) -shared -Wl,-soname,libbooststomp.so.1.0 -o libbooststomp.so.1.0  BoostStomp.o StompFrame.o helpers.o

install: static_lib shared_lib
	install -d $(DESTDIR)/usr/include/booststomp
	install -d $(DESTDIR)/usr/lib
	install libbooststomp.so.1.0 $(DESTDIR)/usr/lib
	install libbooststomp.a $(DESTDIR)/usr/lib
	cp -r *.h $(DESTDIR)/usr/include/booststomp
	cp -r *.hpp $(DESTDIR)/usr/include/booststomp
	
uninstall:
	rm -rf $(DESTDIR)/usr/include/booststomp
	rm -f $(DESTDIR)/usr/lib/libbooststomp*

dist:	main
	rm -f BoostStomp.tar.gz
	tar -c --exclude=".git" --exclude ".svn" --exclude "*.o" -hvzf BoostStomp.tar.gz *.cpp *.h *.hpp Makefile license/ README*

bindist: main
	rm -f BoostStomp_bin_`uname -i`.tar.gz
	tar -c --exclude=".git" --exclude ".svn" -hvzf BoostStomp_bin_`uname -i`.tar.gz main *.a *.so license/ README*

clean:
	rm -f main *.o *.a *.so
