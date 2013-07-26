CC=gcc
#OPT=-O3 -s -Wall
OPT=-O2 -s -Wall
#uncomment the following if you have libjpeg
DEFS=-DJPEG -DNODEBUG
#DEFS=

picturegrabber: picturegrabber.c
	$(CC) $(OPT) $(DEFS) picturegrabber.c -ljpeg -o picturegrabber

clean:  
	rm -f *.o picturegrabber core
