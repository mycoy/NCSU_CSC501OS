#Author: Wenzhao Zhang; UnityID: wzhang27
all: mythread.a

mythread.a: list.o queue.o mythread.o
	ar rcs mythread.a list.o queue.o mythread.o
	
list.o: list.h list.c
	gcc -w -c -o list.o  list.c
	
queue.o: queue.h queue.c
	gcc -w -c -o queue.o queue.c
	
mythread.o : list.h queue.h mythread.c
	gcc -w -c -o mythread.o  mythread.c
	
clean:
	rm -rf mythread.a list.o queue.o mythread.o
	
