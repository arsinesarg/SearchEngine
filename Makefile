CFLAGS = -Wall -Wextra -O2 -std=c++17
HEADERS = text.h html.h thread_pool.h
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	CC := c++
	CURL_FLAGS := -I/opt/homebrew/opt/curl/include -L/opt/homebrew/opt/curl/lib -lcurl
	THREAD_FLAGS :=
else
	CC := g++
	CURL_FLAGS := -lcurl
	THREAD_FLAGS := -lpthread
endif

all: crawler server client

crawler: crawler.cpp $(HEADERS)
	$(CC) $(CFLAGS) crawler.cpp -o crawler $(CURL_FLAGS)

server: server.cpp $(HEADERS)
	$(CC) $(CFLAGS) server.cpp -o server $(THREAD_FLAGS)



client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client

test: test.cpp $(HEADERS)
	$(CC) $(CFLAGS) test.cpp -o test && ./test

run: crawler server
	./crawler && ./server

clean:
	rm -f crawler server client test index.txt urls.txt