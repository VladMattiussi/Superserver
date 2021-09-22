all :		superserver.exe
superserver.exe :	superserver.o 
		gcc -Wpedantic -Wall -o superserver.exe superserver.o
		@echo "";
		./superserver.exe

superserver.o :	superserver.c 
		gcc -c -Wpedantic -Wall superserver.c

clean:		
		-rm    superserver.exe *.o

.PHONY: all superserver.exe


