CC:=gcc
INCLUDES:=$(shell pkg-config --cflags libavformat libavcodec libswresample libswscale libavutil sdl)
CFLAGS:=-Wall -ggdb
LDFLAGS:=$(shell pkg-config --libs libavformat libavcodec libswresample libswscale libavutil sdl) -lm -lpthread
EXE:=write_user.out read_user.out

# This is here to prevent Make from deleting secondary files.
.SECONDARY:
	
# $< is the first dependency in the dependency list
# $@ is the target name
all: $(EXE) tags

tags: *.c
	ctags *.c

%.out: %.o
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

%.o : %.c
	$(CC) $(CFLAGS) $< $(INCLUDES) -c -o $@

clean:
	rm -f *.o *.out
