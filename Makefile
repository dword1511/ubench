TOOLCHAIN_ARM=arm-linux-gnueabi-
TOOLCHAIN_IOS=
TOOLCHAIN_X86=

all: ubench

ubench:
	gcc -c -O3 -Wall -D_GNU_SOURCE ubench.c
	gcc -o ubench ubench.o -lrt
	strip -s ubench

cross: ubench-x86 ubench-x86_64 ubench-ios ubench-arm

ubench-x86:
	$(TOOLCHAIN_X86)gcc -m32 -c -O3 -Wall -D_GNU_SOURCE -o ubench32.o ubench.c
	$(TOOLCHAIN_X86)gcc -m32 -o ubench-x86 ubench32.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86

ubench-x86_64:
	$(TOOLCHAIN_X86)gcc -m64 -c -O3 -Wall -D_GNU_SOURCE -o ubench64.o ubench.c
	$(TOOLCHAIN_X86)gcc -m64 -o ubench-x86_64 ubench64.o -lrt
	$(TOOLCHAIN_X86)strip -s ubench-x86_64

ubench-ios:

ubench-arm:
	$(TOOLCHAIN_ARM)gcc -c -O3 -Wall -D_GNU_SOURCE -o ubencharm.o ubench.c
	$(TOOLCHAIN_ARM)gcc -static -o ubench-arm ubencharm.o -lrt
	$(TOOLCHAIN_ARM)strip -s ubench-arm


clean:
	rm -f *.o
	rm -f ubench ubench-*