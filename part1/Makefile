CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)

http_server: http_server.c http.o
	$(CC) -o $@ $^

http.o: http.c http.h
	$(CC) -c http.c

clean:
	rm -rf *.o http_server

zip:
	@echo "ERROR: You cannot run 'make zip' from the part1 subdirectory. Change to the main proj4-code directory and run 'make zip' there."
