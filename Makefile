CC=gcc
CFLAGS=-std=gnu99 -O3 -static

wsserver: main.o wfws.o wfasyncio.o wfcoroutine.o wfhttp.o sha1.o base64.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c wfws/wfws.h wfasyncio/wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ main.c

wfcoroutine.o: wfcoroutine/wfcoroutine.c wfcoroutine/wfcoroutine.h wfasyncio/wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ wfcoroutine/wfcoroutine.c

wfasyncio.o: wfasyncio/wfasyncio.c wfasyncio/wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ wfasyncio/wfasyncio.c

wfws.o: wfws/wfws.c wfws/wfws.h wfasyncio/wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ wfws/wfws.c

wfhttp.o: wfhttp/wfhttp.c wfhttp/wfhttp.h
	$(CC) $(CFLAGS) -c -o $@ wfhttp/wfhttp.c

sha1.o: wfcrypto/sha1.c
	$(CC) $(CFLAGS) -c -o $@ wfcrypto/sha1.c

base64.o: wfcrypto/base64.c
	$(CC) $(CFLAGS) -c -o $@ wfcrypto/base64.c

clean:
	rm *.o
