TOOLCHAIN_ARM=arm-linux-gnueabi-
TOOLCHAIN_IOS=arm-apple-darwin-
TOOLCHAIN_X86=
PREFIX=/usr/local

DEPS=ubench.c rand.h version.h
CFLAGS=-c -O3 -Wall

all: ubench

ubench: $(DEPS)
	gcc $(CFLAGS) ubench.c
	gcc -o ubench ubench.o -lrt
	strip -s ubench

install: ubench
	install -m 0755 -g root -o root -p ubench $(PREFIX)/bin/

cross: ubench-x86 ubench-x86_64 ubench-ios ubench-arm

ubench-x86: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m32 $(CFLAGS) -o ubench32.o ubench.c
	$(TOOLCHAIN_X86)gcc -m32 -o ubench-x86 ubench32.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86

ubench-x86_64: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m64 $(CFLAGS) -o ubench64.o ubench.c
	$(TOOLCHAIN_X86)gcc -m64 -o ubench-x86_64 ubench64.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86_64

ubench-ios: $(DEPS)
	$(TOOLCHAIN_IOS)gcc $(CFLAGS) -o ubencharm.o ubench.c
	$(TOOLCHAIN_IOS)gcc -static -o ubench-arm ubencharm.o -lrt
	$(TOOLCHAIN_IOS)strip -s ubench-arm

ubench-arm: $(DEPS)
	$(TOOLCHAIN_ARM)gcc $(CFLAGS) -o ubencharm.o ubench.c
	$(TOOLCHAIN_ARM)gcc -static -o ubench-arm ubencharm.o -lrt
	$(TOOLCHAIN_ARM)strip -s ubench-arm

clean:
	rm -f *.o
	rm -f ubench ubench-*