.phony all:
all: diskinfo.c diskget.c disklist.c diskput.c helper.c
	gcc -Wall diskinfo.c helper.c -o diskinfo
	gcc -Wall diskget.c helper.c -o diskget
	gcc -Wall disklist.c helper.c -o disklist
	gcc -Wall diskput.c helper.c -o diskput

info: diskinfo.c helper.c
	gcc -Wall diskinfo.c helper.c -o diskinfo

get: diskget.c helper.c
	gcc -Wall diskget.c helper.c -o diskget

list: disklist.c helper.c
	gcc -Wall disklist.c helper.c -o disklist

put: diskput.c helper.c
	gcc -Wall diskput.c helper.c -o diskput

.PHONY clean:
clean:
	-rm -rf *.o *.exe