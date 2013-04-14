#
# Makefile for BoostStomp
# Elias Karakoulakis <elias.karakoulakis@gmail.com>

# GNU make only

.SUFFIXES:	.cpp .o .a .s

include ../Make-globals

%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

all: main libbooststomp.a libbooststomp.so.$(VERSION)
        	
main:   Main.o  BoostStomp.o StompFrame.o helpers.o
	$(CXX) -o $@ Main.o BoostStomp.o StompFrame.o helpers.o $(LDFLAGS)
#	upx main
	
libbooststomp.a:	BoostStomp.o StompFrame.o
	$(AR) libbooststomp.a BoostStomp.o StompFrame.o helpers.o
ifeq ('$(TARGET)','RELEASE')
	strip libbooststomp.a
endif
	
libbooststomp.so.$(VERSION): 
	$(CXX) -o libbooststomp.so.$(VERSION) BoostStomp.o StompFrame.o helpers.o \
	-shared -Wl,-soname,libbooststomp.so.$(VERSION) $(LDFLAGS)
ifeq ('$(TARGET)','RELEASE')
	strip libbooststomp.so*
endif
