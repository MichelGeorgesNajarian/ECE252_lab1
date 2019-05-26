#########################################################################
# ECE252 Lab Makefile
# Y. Huang, 2018/10/15
#########################################################################

CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g


OBJS=findpng.c

all: findpng.out

findpng.out: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

.PHONY: clean
clean:
	rm -f *.d *.o *.out 
