all: build

build:
	gcc lwm.c -o main -Wall -Wextra -std=c17 -lX11
