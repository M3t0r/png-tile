LDFLAGS = `pkg-config --libs libpng ncurses` -lm
CFLAGS = -std=c99 `pkg-config --cflags libpng ncurses`

png-tile: tile.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

.PHONY: clean
clean:
	-rm *.o png-tile