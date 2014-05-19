#Author: Wenzhao Zhang; UnityID: wzhang27
all: ush

ush: parse.o ush.o
	gcc -w -o ush parse.o ush.o
	
parse.o: parse.h parse.c
	gcc -w -c -o parse.o parse.c
	
ush.o: parse.h ush.c
	gcc -w -c -o ush.o ush.c
	
clean:
	rm -rf parse.o ush.o ush
