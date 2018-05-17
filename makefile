Testcase_OS_2.exe : testcase.c disk.o fs.o mount.o
	gcc -o Testcase_OS_2.exe testcase.c disk.o fs.o mount.o
testcase.o : mount.c disk.c fs.c fs.h disk.h
	gcc -c -o testcase.o testcase.c
disk.o : disk.h disk.c
	gcc -c -o disk.o disk.c
fs.o : fs.h fs.c
	gcc -c -o fs.o fs.c
mount.o : mount.c
	gcc -c -o mount.o mount.c
clean : 
	rm -rf testcase.o disk.o fs.o mount.o
