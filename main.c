#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "util.h"
#include "tile.h"

char *VERSION_STRING = "png-tile v0.1";

int main(int argc, char *argv[]) {
    const char
        *TILE_FILENAME_FORMAT = "tile_%i_%i.png";
    const unsigned int
        TILE_SIZE = 256, //px
        TILE_FILENAME_INDEX_BEGIN = 0,
        TILE_FILENAME_INCREMENT = 2;

    // initialization for fancy output
    fancy_init();

    if(argc < 2) {
        printf("Usage: %s <image_path.png> [output_directory]\n", argv[0]);
        return 1;
    }

    // parse cmdline options to variables
    char
        *src_path = argv[1],
        *dst_dir_path = "./tiles/";
    if(argc > 2) {
        dst_dir_path = argv[2];
    }

    // output directory vorbereiten
    if(mkdir(dst_dir_path, 0777) == -1 && errno != EEXIST) {
        // 0777 wird durch die umask des prozesses nach oben limitiert
        return perr("Could not create directory \"%s\"\n", dst_dir_path);
    }

    tile_file_path_format_struct tile_file_path_format;

    tile_file_path_format.format_string = salloc(strlen(dst_dir_path)+1+strlen(TILE_FILENAME_FORMAT)+1);
    strcpy(tile_file_path_format.format_string, dst_dir_path);
    // append / if it is not the last char in tile_file_path_format.format_string
    if(tile_file_path_format.format_string[strlen(tile_file_path_format.format_string)-1] != '/')
    		strcat(tile_file_path_format.format_string, "/");
    strcat(tile_file_path_format.format_string, TILE_FILENAME_FORMAT);

    tile_file_path_format.buffer_size     = strlen(tile_file_path_format.format_string)+32;
    tile_file_path_format.index_begin     = 0;
    tile_file_path_format.index_increment = 2;

	return tile_file(src_path, tile_file_path_format, TILE_SIZE);
}
