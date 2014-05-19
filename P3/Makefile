#Author: Wenzhao Zhang; UnityID: wzhang27
all: master player

master: master.o
	gcc -w -o master master.o
	
player: player.o
	gcc -w -o player player.o
	
master.o: master.c
	gcc -w -c -o master.o master.c
	
player.o: player.c
	gcc -w -c -o player.o player.c
	
clean:
	rm -rf master.o player.o master player
