CC := gcc
AR := ar

CFLAGS += -fPIC

HEADERS = ctar.h tfs.h
OBJS = tfs.o

all: libtfs.a libtfs.so

libtfs.a: $(OBJS) Makefile
	$(AR) rcs $@ $(OBJS)

libtfs.so: $(OBJS) Makefile
	$(CC) -shared -o $@ $(OBJS)


%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c -o $@ $<
