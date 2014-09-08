LDFLAGS = `pkg-config --libs libpng ncurses` -lm
CFLAGS = -std=c99 `pkg-config --cflags libpng ncurses`

png-tile: main.o tile.o util.o
	$(CC) $(LDFLAGS) -o $@ $+

.PHONY: clean
clean:
	-rm *.o png-tile
