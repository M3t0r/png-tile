#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define __USE_BSD // damit usleep deklariert wird, im endügltigen produkt rausnehmen
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <curses.h>
#include <term.h>

#include <png.h>
// die doku meint zlib bräuchte man, wenn's klappt mal ohne probieren...
#include <zlib.h>

char *VERSION_STRING = "png-tile v0.1";

int b_is_interactive;
int i_term_columns;

int save_tile(const char *cpc_dst_path, png_bytep p_dst_rows[], const int ci_src_color_type, const int ci_src_bit_depth, const unsigned int cui_dst_width, const unsigned int cui_dst_height);
void progress_meter(char *job, double progress);
int perr(const char *format, ...);
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int main(int argc, char **argv) {
    const unsigned int
        TILE_SIZE = 256, //px
        TILE_NAME_BEGIN = 0,
        TILE_NAME_INCREMENT = 2,
        DST_PATH_BUFFER_SIZE = 64;
    const char
        *DST_PATH_FORMAT = "tile_%i_%i.png";

    // initsteps for fancy output
    b_is_interactive = isatty(STDOUT_FILENO);
    if(b_is_interactive) {
        setterm(NULL);
        i_term_columns = tgetnum("cols");
        if(i_term_columns < 0) i_term_columns = 80;
    }

    if(argc < 2) {
        printf("Usage: %s <image_path.png> [output_directory]\n", argv[0]);
        return 1;
    }

    // cmdline options in variablen speichern
    char
        *pc_src_path = argv[1],
        *pc_dst_dir_path = "tiles";
    if(argc > 2) {
        pc_dst_dir_path = argv[2];
    }

    // die datei öffnen
    FILE *pf_src = fopen(pc_src_path, "rb");
    if(!pf_src) {
        return perr("Could not open file \"%s\"\n", pc_src_path);
    }

    // output directory vorbereiten
    if(mkdir(pc_dst_dir_path, 0777) == -1 && errno != EEXIST) { // 0777 wird durch die umask des prozesses nach oben limitiert
        return perr("Could not create directory \"%s\"\n", pc_dst_dir_path);
    }
    if(chdir(pc_dst_dir_path) == -1) {
        return perr("Could not use \"%s\" as output directory\n", pc_dst_dir_path);
    }

    // prüfen ob es sich um ein PNG handelt
    png_byte pb_src_header[8];
    fread(pb_src_header, 1, 8, pf_src);
    if(png_sig_cmp(pb_src_header, 0, 8)) {
        return perr("\"%s\" is not a png file.\n", pc_src_path);
    }

    // das png_structp erzeugen (speichert den context für libpng)
    png_structp ppng_src = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!ppng_src) {
        return perr("Could not allocate ppng_src\n");
    }

    // png_infop erzeugen. hält metadaten aus chuncks vor dem eigentlichen bilddaten
    png_infop pinfo_src = png_create_info_struct(ppng_src);
    if(!pinfo_src) {
        return perr("Could not allocate pinfo_src\n");
    }

    // crazy error handling via setjmp/longjmp. is ja fast wie goto...
    if(setjmp(png_jmpbuf(ppng_src))) {
        return perr("An error was encountered while reading th+e image. "
               "Sorry we couldn't be more specific.\n");
    }

    // filehandle an context übergeben und mitteilen das wir schon 8 byte für dateityp überprüfung verwendet haben
    png_init_io(ppng_src, pf_src);
    png_set_sig_bytes(ppng_src, 8);

    // die info chunks in pinfo_src lesen
    png_read_info(ppng_src, pinfo_src);

    // prüfen ob interlaced, wenn ja dann beenden
    if(png_get_interlace_type(ppng_src, pinfo_src) != PNG_INTERLACE_NONE) {
        return perr("The image is interlaced. We cannot handle interlaced images for incremental slicing. Sorry.\n");
    }

    // die metadaten in variablen speichern und ein bisschen info ausgeben
    const unsigned int
        cui_src_height       = png_get_image_height(ppng_src, pinfo_src),
        cui_src_width        = png_get_image_width (ppng_src, pinfo_src),
        cui_src_rowsize      = png_get_rowbytes(ppng_src, pinfo_src),
        cui_src_data_size    = TILE_SIZE*cui_src_rowsize,
        cui_src_bytes_per_px = cui_src_rowsize / cui_src_width,
        cui_x_tiles          = (cui_src_width  / TILE_SIZE) + ((cui_src_width  % TILE_SIZE == 0) ? 0 : 1),
        cui_y_tiles          = (cui_src_height / TILE_SIZE) + ((cui_src_height % TILE_SIZE == 0) ? 0 : 1),
        cui_num_tiles        = cui_x_tiles*cui_y_tiles;
    const int
        ci_src_color_type    = png_get_color_type(ppng_src, pinfo_src),
        ci_src_bit_depth     = png_get_bit_depth(ppng_src, pinfo_src);
    printf(
        "Image dimensions are %ix%i. Generating a %ix%i matrix with %i tiles. %i\n",
        cui_src_width, cui_src_height, cui_x_tiles, cui_y_tiles, cui_num_tiles, cui_src_bytes_per_px
    );
    printf(
        "With a tile-size of %ipx, incremental slizing will need %.3fMiB of memory.\n",
        TILE_SIZE, cui_src_data_size/1024.0/1024
    );
    if(b_is_interactive) {
        printf("\n");
        progress_meter("Saving Tiles", 0);
    }
    
    // die großen speicher bereiche deklarieren und initialisieren
    png_bytep p_src_rows[cui_src_height];
    png_bytep p_dst_rows[TILE_SIZE];
    png_bytep p_src_data = calloc(cui_src_rowsize, TILE_SIZE);
    for(int i = 0; i < cui_src_height; i++)
        p_src_rows[i] = p_src_data + (cui_src_rowsize * (i%TILE_SIZE));

    // jetzt TILE_SIZE zeilen einlesen, solange bis wir alle zeilen haben
    unsigned int x, y, ui_rows_to_read,
        ui_rows_left = cui_src_height;
    for(y = 0; y < cui_y_tiles; y++) {

        // wieviele zeilen KÖNNEN wir lesen?
        if(ui_rows_left < TILE_SIZE)
            ui_rows_to_read = ui_rows_left;
        else
            ui_rows_to_read = TILE_SIZE;

        // lese ui_rows_to_read zeilen in speicher
        png_read_rows(ppng_src, p_src_rows, NULL, ui_rows_to_read);
        // aktualisieren
        ui_rows_left -= ui_rows_to_read;

        for(x = 0; x < cui_x_tiles; x++) {
            char pc_dst_path[DST_PATH_BUFFER_SIZE];
            snprintf(pc_dst_path, DST_PATH_BUFFER_SIZE, DST_PATH_FORMAT, x, y);

            // p_dst_rows mit x_offset füllen
            const unsigned int cui_x_offset = cui_src_bytes_per_px * x * TILE_SIZE;
            for(int i = 0; i < TILE_SIZE && i < ui_rows_to_read; i++)
                p_dst_rows[i] = p_src_rows[i] + cui_x_offset;

            save_tile(pc_dst_path, p_dst_rows, ci_src_color_type, ci_src_bit_depth, MIN(cui_src_width - (x*TILE_SIZE), TILE_SIZE), ui_rows_to_read);

            progress_meter("Saving Tiles", (y*cui_x_tiles + x)/(double)cui_num_tiles);
        }
    }
    if(b_is_interactive) {
        progress_meter("Saving Tiles", 1);
        printf("\n");
    }

    // cleanup
    png_read_end(ppng_src, (png_infop)NULL); // die letzten chunks lesen, ordnungs halber
    png_destroy_read_struct(&ppng_src, &pinfo_src, (png_infopp)NULL);
    free(p_src_data);
    fclose(pf_src);

    return 0;
}

int save_tile(const char *cpc_dst_path, png_bytep p_dst_rows[], const int ci_src_color_type, const int ci_src_bit_depth, const unsigned int cui_dst_width, const unsigned int cui_dst_height)
{
    FILE *pf_dst = fopen(cpc_dst_path, "wb");
    if(!pf_dst) {
        return perr("Could not open \"%s\" for writing", cpc_dst_path);
    }

    png_structp ppng_dst = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!ppng_dst)
        return perr("Could not create ppng_dst.\n");

    png_infop pinfo_dst = png_create_info_struct(ppng_dst);
    if(!pinfo_dst)
        return perr("Could not create pinfo_dst.\n");

    // goto-style error handling...
    if(setjmp(png_jmpbuf(ppng_dst))) {
        return perr("Error while writing to file \"%s\".\n", cpc_dst_path);
    }

    png_init_io(ppng_dst, pf_dst);

    png_set_IHDR(
        ppng_dst, pinfo_dst,
        cui_dst_width, cui_dst_height,
        ci_src_bit_depth, ci_src_color_type,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );

//    {
//        png_text t_software;
//        char *key="Software";
//        t_software.key = key;
//        t_software.text = VERSION_STRING;
//        t_software.compression = PNG_TEXT_COMPRESSION_NONE;
//        t_software.itxt_length = 0;
//        t_software.lang = NULL;
//        t_software.lang_key = NULL;
//
//        png_set_text(ppng_dst, pinfo_dst, &t_software, 1);
//    }

    png_set_rows(ppng_dst, pinfo_dst, p_dst_rows);

    png_write_png(ppng_dst, pinfo_dst, 0, NULL);

    fclose(pf_dst);
}

void progress_meter(char *job, double progress) {
    if(b_is_interactive) {
        int pbar_length = i_term_columns - strlen(job) - 13;
        int pbar_num_full = ceil(pbar_length*progress);

        char *pbar = malloc(pbar_length + 1);
        memset(pbar, '#', pbar_num_full);
        memset(pbar+pbar_num_full, ' ', pbar_length - pbar_num_full);
        pbar[pbar_length] = 0;

        printf("%s: [%s] % 3.2f%%\r", job, pbar, progress*100);
        fflush(stdout);
    }
}

int perr(const char *format, ...) {
    va_list va_list;
    va_start(va_list, format);
    vfprintf(stderr, format, va_list);
    va_end(va_list);
    return 1;
}
