#
# Makefile for JAudioPlayer
#
# Author: Jay Biernat
#

CC = gcc
CFLAGS = -Wall -O2
DEPS = JAudioPlayer.h JPlayerGUI.h
ODIR = obj
SDL_IDIR = /usr/include/SDL2
_OBJ = JPlayerGUI.o JAudioPlayer.o main.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
LIBS = -lportaudio -lsndfile -lSDL2 -lpthread
OUT_EXE = JAudioPlayer

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -I$(SDL_IDIR) -c -o $@ $<

build: $(OBJ)
	$(CC) -Wall -o bin/$(OUT_EXE) $^ $(LIBS) -s

clean:
	rm $(ODIR)/*.o
