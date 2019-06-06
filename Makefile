all: blaster blastee

debug: blaster.c packet.h blastee.c
	gcc -Wall -g -o blaster blaster.c
	gcc -Wall -g -o blastee blastee.c

blaster: blaster.c packet.h
	gcc -Wall -O -o blaster blaster.c
blastee: blastee.c packet.h
	gcc -Wall -O -o blastee blastee.c
clean:
	rm -f blaster blastee

