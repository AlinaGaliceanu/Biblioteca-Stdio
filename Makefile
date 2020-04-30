all:build

so_stdio.o: so_stdio.c
	gcc -c -Wall -fPIC so_stdio.c

build: so_stdio.o
	gcc -shared so_stdio.o -o libso_stdio.so
clean:
	rm -f ../checker-lin/libso_stdio.so so_stdio.o
	