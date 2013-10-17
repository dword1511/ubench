PREFIX = /usr/local
CC     = $(CROSS_COMPILE)gcc
STRIP  = $(CROSS_COMPILE)strip
SRCS   = ubench.c
CFLAGS = -c -O3 -Wall

########################################################################
# Variables and default target
OBJS   = $(SRCS:.c=.o)
DEPS   = $(SRCS:.c=.d)
-include $(DEPS)

ubench: $(OBJS)
	gcc -o ubench $(OBJS) -lrt
	$(STRIP) -s ubench

########################################################################
# PHONY targets
.PHONY: install clean

install: ubench
	install -m 0755 -g root -o root -p ubench $(PREFIX)/bin/

clean:
	rm -f *.o
	rm -f $(OBJS) $(DEPS) ubench

########################################################################
# Automatic prequisities
.d: .c
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
