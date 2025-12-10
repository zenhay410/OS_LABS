CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -fPIC -O2
LDFLAGS = -ldl -lm

all: libimpl1.so libimpl2.so prog_static prog_dynamic

libimpl1.so: impl1.c calc.h
	$(CC) $(CFLAGS) -shared impl1.c -o libimpl1.so -lm

libimpl2.so: impl2.c calc.h
	$(CC) $(CFLAGS) -shared impl2.c -o libimpl2.so -lm

prog_static: prog_static.c calc.h libimpl1.so
	$(CC) -Wall -Wextra prog_static.c -L. -limpl1 -o prog_static -lm

prog_dynamic: prog_dynamic.c calc.h libimpl1.so libimpl2.so
	$(CC) -Wall -Wextra prog_dynamic.c -o prog_dynamic $(LDFLAGS)

clean:
	rm -f *.o *.so prog_static prog_dynamic
