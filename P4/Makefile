#Author: Wenzhao Zhang; UnityID: wzhang27; OS_P4
all: ramdisk

ramdisk: ramdisk_log.h ramdisk_log.c ramdisk.h ramdisk_list.c ramdisk.c
	gcc -w ramdisk_log.c ramdisk_list.c ramdisk.c -o ramdisk `pkg-config fuse --cflags --libs` 

clean:
	rm -rf ramdisk_log.o ramdisk_list.o ramdisk.o ramdisk

