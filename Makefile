CC=gcc
CFLAGS=-std=gnu99 -O3 -static

wsserver: main.o wfws.o wfasyncio.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c wfws.h wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ main.c

wfws.o: wfws.c wfws.h wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ wfws.c

wfasyncio.o: wfasyncio.c wfasyncio.h
	$(CC) $(CFLAGS) -c -o $@ wfasyncio.c

clean:
	rm *.o