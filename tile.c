#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "tile.h"
#include "util.h"

int tile_file(char *src_file_path, const char *tile_file_path_format, const uint tile_size) {
    const uint TILE_FILE_PATH_BUFFER_SIZE = strlen(tile_file_path_format)+32;

    // open the file
    FILE *src_fd = fopen(src_file_path, "rb");
    if(!src_fd) {
        return perr("Could not open file \"%s\"\n", src_file_path);
    }

    // check the filesignature for PNG data
    byte src_header[8];
    fread(src_header, 1, 8, src_fd);
    if(png_sig_cmp(src_header, 0, 8)) {
        return perr("\"%s\" is not a png file.\n", src_file_path);
    }

    // create png_structp (libpng context struct)
    png_structp src_ppng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!src_ppng) {
        return perr("Could not allocate src_ppng\n");
    }

    // create png_infop. holds metadata of the png file
    png_infop src_pinfo = png_create_info_struct(src_ppng);
    if(!src_pinfo) {
        return perr("Could not allocate src_pinfo\n");
    }

    // crazy error handling via setjmp/longjmp. looks a lot like goto...
    if(setjmp(png_jmpbuf(src_ppng))) {
        return perr("An error was encountered while reading the image. "
                    "Sorry we couldn't be more specific.\n");
    }

    // give filehandle to the context and note that we already used 8 bytes for
    // the filetype check
    png_init_io(src_ppng, src_fd);
    png_set_sig_bytes(src_ppng, 8);

    // read info chunks of the png in front of the actual image data
    png_read_info(src_ppng, src_pinfo);

    // check if interlaced. if so, we have to abort...
    if(png_get_interlace_type(src_ppng, src_pinfo) != PNG_INTERLACE_NONE) {
        return perr("The image is interlaced. We cannot use interlaced "
                    "images for incremental slicing. Sorry.\n");
    }

    // save metadata for convenient access
    const uint
        src_height       = png_get_image_height(src_ppng, src_pinfo),
        src_width        = png_get_image_width (src_ppng, src_pinfo),
        src_rowsize      = png_get_rowbytes(src_ppng, src_pinfo),
        src_bytes_per_px = src_rowsize / src_width, // lets just hope the data realy aligns on byte borders...
        n_tiles_x        = (src_width  / tile_size)
                            + ((src_width  % tile_size == 0) ? 0 : 1),
        n_tiles_y        = (src_height / tile_size) 
                            + ((src_height % tile_size == 0) ? 0 : 1),
        n_tiles          = n_tiles_x*n_tiles_y,
        src_color_type    = png_get_color_type(src_ppng, src_pinfo),
        src_bit_depth     = png_get_bit_depth(src_ppng, src_pinfo);

    // a little bit of information for the user
    printf(
        "Image dimensions are %ix%i. Generating a %ix%i matrix with %i tiles.\n",
        src_width, src_height, n_tiles_x, n_tiles_y, n_tiles
    );
    printf(
        "With a tile-size of %ipx, incremental slizing will need %.3fMiB of memory.\n",
        tile_size, tile_size*src_rowsize/1024.0/1024
    );


    // now begins the fun part!

    if(term_is_interactive) {
        printf("\n");
        progress_meter("Saving Tiles", 0);
    }
    
    // declare and initialize the big memory blobs
    png_bytep src_row_p[src_height];
    png_bytep tile_row_p[tile_size];
    png_bytep src_data = calloc(src_rowsize, tile_size);
    if(src_data == NULL) {
        return perr("Could not allocate memory for incrementaly reading \"%s\".", src_file_path);
    }
    // libpng needs a pointer to every row of the image but if we only read
    // tile_size many at a time, it won't notice that they all point to the same blob...
    for(int i = 0; i < src_height; i++)
        src_row_p[i] = src_data + (src_rowsize * (i%tile_size));

    // read tile_size rows at a time until all rows have been read
    uint x, y, rows_for_next_pass,
        rows_unread = src_height;
    for(y = 0; y < n_tiles_y; y++) {

        // how man rows CAN we read?
        if(rows_unread < tile_size)
            rows_for_next_pass = rows_unread;
        else
            rows_for_next_pass = tile_size;

        // read the next set of rows into memory
        png_read_rows(src_ppng, src_row_p, NULL, rows_for_next_pass);
        rows_unread -= rows_for_next_pass;

        // slize into tiles
        for(x = 0; x < n_tiles_x; x++) {
            char tile_file_path[TILE_FILE_PATH_BUFFER_SIZE];
            snprintf(tile_file_path, TILE_FILE_PATH_BUFFER_SIZE, tile_file_path_format, x, y);

            // fill tile row pointers with values according to the actual x-offset
            const unsigned int x_offset = src_bytes_per_px * x * tile_size;
            for(int i = 0; i < tile_size && i < rows_for_next_pass; i++)
                tile_row_p[i] = src_row_p[i] + x_offset;

            int success = save_tile(
                tile_file_path, tile_row_p, src_color_type, src_bit_depth,
                MIN(src_width - (x*tile_size), tile_size), rows_for_next_pass
            );

            // abort if one tile could not be safed
            if(success == EXIT_FAILURE)
                return EXIT_FAILURE;

            progress_meter("Saving Tiles", (y*n_tiles_x + x)/(double)n_tiles);
        }
    }
    if(term_is_interactive) {
        progress_meter("Saving Tiles", 1);
        printf("\n");
    }

    // cleanup
    png_read_end(src_ppng, (png_infop)NULL); // read the last chunks that might hold metadata we are not interested in
    png_destroy_read_struct(&src_ppng, &src_pinfo, (png_infopp)NULL);
    free(src_data);
    fclose(src_fd);

    return 0;
}

int save_tile(
    const char *tile_file_path, byte **tile_row_p, const int src_color_type,
    const int src_bit_depth, const uint tile_width,
    const uint tile_height
) {
    FILE *pf_dst = fopen(tile_file_path, "wb");
    if(!pf_dst) {
        return perr("Could not open \"%s\" for writing", tile_file_path);
    }

    png_structp ppng_dst = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!ppng_dst)
        return perr("Could not create ppng_dst.\n");

    png_infop pinfo_dst = png_create_info_struct(ppng_dst);
    if(!pinfo_dst)
        return perr("Could not create pinfo_dst.\n");

    // goto-style error handling...
    if(setjmp(png_jmpbuf(ppng_dst))) {
        return perr("Error while writing to file \"%s\".\n", tile_file_path);
    }

    png_init_io(ppng_dst, pf_dst);

    png_set_IHDR(
        ppng_dst, pinfo_dst,
        tile_width, tile_height,
        src_bit_depth, src_color_type,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );

    {
        png_text t_software;
        char *key="Software";
        t_software.key = key;
        t_software.text = VERSION_STRING;
        t_software.compression = PNG_TEXT_COMPRESSION_NONE;
        t_software.itxt_length = 0;
        t_software.lang = NULL;
        t_software.lang_key = NULL;

        png_set_text(ppng_dst, pinfo_dst, &t_software, 1);
    }

    png_set_rows(ppng_dst, pinfo_dst, tile_row_p);

    png_write_png(ppng_dst, pinfo_dst, 0, NULL);

    fclose(pf_dst);
}
