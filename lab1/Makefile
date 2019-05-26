#########################################################################
# ECE252 Lab Makefile
# Y. Huang, 2018/10/15
#########################################################################

CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g


OBJS1=findpng.o
OBJS2=catpng.o

all: findpng.out catpng.out

findpng.out: $(OBJS1) 
	$(LD) $(LDFLAGS) -o $@ $^


catpng.out: $(OBJS2) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

.PHONY: clean
clean:
	rm -f *.d *.o *.out 
