TOOLCHAIN_ARM    = arm-linux-gnueabi-
TOOLCHAIN_IOS    = arm-apple-darwin-
TOOLCHAIN_X86    =
TOOLCHAIN_MIPS   = mips-openwrt-linux-
TOOLCHAIN_MIPSEL = mipsel-openwrt-linux-

PREFIX           = /usr/local

########################################################################

DEPS   = ubench.c rand.h version.h
CFLAGS = -c -O3 -Wall

all: ubench

ubench: $(DEPS)
	gcc $(CFLAGS) ubench.c
	gcc -o ubench ubench.o -lrt
	strip -s ubench

install: ubench
	install -m 0755 -g root -o root -p ubench $(PREFIX)/bin/

cross: ubench-x86 ubench-x86_64 ubench-ios ubench-arm

ubench-x86: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m32 $(CFLAGS) -o ubench-x86.o ubench.c
	$(TOOLCHAIN_X86)gcc -m32 -o ubench-x86 ubench-x86.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86

ubench-x86_64: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m64 $(CFLAGS) -o ubench-x86_64.o ubench.c
	$(TOOLCHAIN_X86)gcc -m64 -o ubench-x86_64 ubench-x86_x64.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86_64

ubench-ios: $(DEPS)
	$(TOOLCHAIN_IOS)gcc $(CFLAGS) -DEMBEDDED -o ubench-ios.o ubench.c
	$(TOOLCHAIN_IOS)gcc -static -o ubench-ios ubench-ios.o -lrt
	$(TOOLCHAIN_IOS)strip -s ubench-ios

ubench-arm: $(DEPS)
	$(TOOLCHAIN_ARM)gcc $(CFLAGS) -DEMBEDDED -o ubench-arm.o ubench.c
	$(TOOLCHAIN_ARM)gcc -static -o ubench-arm ubench-arm.o -lrt
	$(TOOLCHAIN_ARM)strip -s ubench-arm

ubench-mips: $(DEPS)
	$(TOOLCHAIN_MIPS)gcc $(CFLAGS) -DEMBEDDED -o ubench-mips.o ubench.c
	$(TOOLCHAIN_MIPS)gcc -static -o ubench-mips ubench-mips.o -lrt
	$(TOOLCHAIN_MIPS)strip -s ubench-mips

ubench-mipsel: $(DEPS)
	$(TOOLCHAIN_MIPSEL)gcc $(CFLAGS) -DEMBEDDED -o ubench-mipsel.o ubench.c
	$(TOOLCHAIN_MIPSEL)gcc -static -o ubench-mipsel ubench-mipsel.o -lrt
	$(TOOLCHAIN_MIPSEL)strip -s ubench-mipsel

clean:
	rm -f *.o
	rm -f ubench ubench-*
