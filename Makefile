TOOLCHAIN_ARM=arm-linux-gnueabi-
TOOLCHAIN_IOS=arm-apple-darwin-
TOOLCHAIN_X86=

DEPS=ubench.c rand.h version.h

all: ubench

ubench: $(DEPS)
	gcc -c -O3 -Wall -D_GNU_SOURCE ubench.c
	gcc -o ubench ubench.o -lrt
	strip -s ubench

install: ubench
	cp ubench /usr/local/bin/

cross: ubench-x86 ubench-x86_64 ubench-ios ubench-arm

ubench-x86: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m32 -c -O3 -Wall -D_GNU_SOURCE -o ubench32.o ubench.c
	$(TOOLCHAIN_X86)gcc -m32 -o ubench-x86 ubench32.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86

ubench-x86_64: $(DEPS)
	$(TOOLCHAIN_X86)gcc -m64 -c -O3 -Wall -D_GNU_SOURCE -o ubench64.o ubench.c
	$(TOOLCHAIN_X86)gcc -m64 -o ubench-x86_64 ubench64.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86_64

ubench-ios: $(DEPS)
	$(TOOLCHAIN_IOS)gcc -c -O3 -Wall -D_GNU_SOURCE -o ubencharm.o ubench.c
	$(TOOLCHAIN_IOS)gcc -static -o ubench-arm ubencharm.o -lrt
	$(TOOLCHAIN_IOS)strip -s ubench-arm

ubench-arm: $(DEPS)
	$(TOOLCHAIN_ARM)gcc -c -O3 -Wall -D_GNU_SOURCE -o ubencharm.o ubench.c
	$(TOOLCHAIN_ARM)gcc -static -o ubench-arm ubencharm.o -lrt
	$(TOOLCHAIN_ARM)strip -s ubench-arm


clean:
	rm -f *.o
	rm -f ubench ubench-*