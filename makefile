#MAKEFILE for Project
CC=gcc
CFLAGS=-m32

project: type.h project.c util.c
	$(CC) $(CFLAGS) type.h project.c util.c -o project

debug: type.h project.c util.c
	$(CC) $(CFLAGS) -D DEBUG type.h project.c util.c -o dproject

#util.o: util.c
#	$(CC) $(CFLAGS) util.c
