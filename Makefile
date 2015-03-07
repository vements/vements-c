test: libvements.c
	gcc -g -lcurl -Ijansson-master/src jansson-master/src/.libs/libjansson.a libvements.c -o test

clean:
	rm test
