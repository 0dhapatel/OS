CC = gcc
CFLAGS = -g -c -m32
AR = ar -rc
RANLIB = ranlib

all: vm.a

vm.a: vm.o
	$(AR) libvm.a vm.o
	$(RANLIB) libvm.a

vm.o: vm.h

	$(CC) $(CFLAGS)  vm.c

clean:
	rm -rf *.o *.a
