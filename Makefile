#
# Makefile for BoostStomp
# Elias Karakoulakis <elias.karakoulakis@gmail.com>

# GNU make only

.SUFFIXES:	.cpp .o .a .s

CC     := gcc
CXX    := g++
LD     := g++
AR     := ar rc
RANLIB := ranlib

# Change for DEBUG or RELEASE
TARGET := DEBUG

DEBUG_CFLAGS    := -Wall -Wno-format -g -DDEBUG -Werror -O0 -DDEBUG_STOMP
RELEASE_CFLAGS  := -Wall -Wno-unknown-pragmas -Wno-format -O3 -DNDEBUG

DEBUG_LDFLAGS	:= -g

CFLAGS	:= -c $($(TARGET)_CFLAGS) 
LDFLAGS	:= $($(TARGET)_LDFLAGS) -L/usr/lib/ -L/usr/local/lib -lboost_system -lboost_thread
INCLUDES := -I .

%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

all: main
        
BoostStomp.o:  BoostStomp.cpp BoostStomp.hpp StompFrame.hpp
	g++ $(CFLAGS) -c BoostStomp.cpp $(INCLUDES)    

Main.o: Main.cpp 
	g++ $(CFLAGS) -c Main.cpp $(INCLUDES)  
	
main:   Main.o  BoostStomp.o 
	$(LD) -o $@ $(LDFLAGS) Main.o BoostStomp.o
#	upx main
	
dist:	main
	rm -f BoostStomp.tar.gz
	tar -c --exclude=".git" --exclude ".svn" --exclude "*.o" -hvzf BoostStomp.tar.gz *.cpp *.h *.hpp Makefile license/ README*

bindist: main
	rm -f BoostStomp_bin_`uname -i`.tar.gz
	tar -c --exclude=".git" --exclude ".svn" -hvzf BoostStomp_bin_`uname -i`.tar.gz main *.a *.so license/ README*

clean:
	rm -f main *.o 
