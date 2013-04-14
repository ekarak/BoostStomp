#
# Makefile for BoostStomp
# Elias Karakoulakis <elias.karakoulakis@gmail.com>

# GNU make only

.SUFFIXES:	.cpp .o .a .s
.PHONY: all

include Make-globals

all: 
	$(MAKE) -C src/

install: 
	$(MAKE) -C src/
	install -d $(DESTDIR)/include/booststomp
	install -d $(DESTDIR)/lib
	install src/libbooststomp.so.$(VERSION) $(DESTDIR)/lib
	ln -sf  $(DESTDIR)/lib/libbooststomp.so.$(VERSION) $(DESTDIR)/lib/libbooststomp.so 
	install src/libbooststomp.a $(DESTDIR)/lib
	cp -r src/*.h   $(DESTDIR)/include/booststomp
	cp -r src/*.hpp $(DESTDIR)/include/booststomp
	echo "Dont forget to run: "
	echo "    sudo ldconfig $(DESTDIR)/lib "
	
uninstall:
	rm -rf $(DESTDIR)/include/booststomp
	rm -f $(DESTDIR)/lib/libbooststomp*

dist:	main
	rm -f BoostStomp.tar.gz
	tar -c --exclude-vcs --exclude "*.o" -hvzf BoostStomp.tar.gz *.cpp *.h *.hpp Makefile license/ README*

bindist: main
	rm -f BoostStomp_bin_`uname -i`.tar.gz
	tar -c --exclude=".git" --exclude ".svn" -hvzf BoostStomp_bin_`uname -i`.tar.gz main *.a *.so license/ README*

valgrind-test:
	valgrind src/main

test: valgrind-test

clean:
	cd src; rm -f main *.o *.a libbooststomp.so*

deb:
	git clone --depth 0 git://github.com/ekarak/BoostStomp.git libbooststomp-$(VERSION)
	tar --exclude-vcs -czvf libbooststomp_$(VERSION).orig.tar.gz libbooststomp-$(VERSION)
	cd libbooststomp-$(VERSION) && dpkg-buildpackage
	rm -rf libbooststomp-$(VERSION)