.phony all:
all: ACS

ACS: main.c queue.c
	gcc -Wall -pthread main.c queue.c -o ACS

.PHONY clean:
clean:
	-rm -rf *.o *.exe