
# Makefile, ECE252  
# Yiqing Huang, 2018/11/04

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flg 
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

# For students 
LIB_UTIL = zutil.o crc.o
SRCS   = catpng.c crc.c zutil.c
OBJS   = catpng.o $(LIB_UTIL) 
OBJS1  = findpng.o

TARGETS= catpng.out 

all: ${TARGETS} findpng.out

catpng.out: $(OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

findpng.out: $(OBJS1)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
