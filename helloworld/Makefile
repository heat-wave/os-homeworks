CC=gcc

all: main libhello.so

main:
	$(CC) main.c -o main.o -c
	$(CC) -s main.o -o main -L. -lhello

libhello.so:
	$(CC) hello.c -o hello.o -c
	ar rcs libhello.a hello.o
	$(CC) -fPIC hello.c -c -o hello.o
	$(CC) -shared hello.o -o libhello.so

clean:
	rm *.o *.a *.so