#
# Makefile for JAudioPlayer
#
# Author: Jay Biernat
#

CC = gcc
CFLAGS = -Wall -O2
DEPS = JAudioPlayer.h JPlayerGUI.h
ODIR = obj
_OBJ = JPlayerGUI.o JAudioPlayer.o main.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
LIBS = -lportaudio -lsndfile -lSDL2 -lpthread
OUT_EXE = bin/JAudioPlayer

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

build: clean $(OBJ)
	$(CC) -Wall -o $(OUT_EXE) $(OBJ) $(LIBS) -s

clean:
	rm -f $(ODIR)/*.o $(OUT_EXE)
