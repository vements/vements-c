test: libvements.c
	gcc -g -lcurl libvements.c -o test

clean:
	rm test
