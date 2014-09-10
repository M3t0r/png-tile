#ifndef __HEADER_TILE_H
#define __HEADER_TILE_H
#include "util.h"

typedef struct __tile_file_path_format_struct {
    uint  index_begin;
    uint  index_increment;
    uint  buffer_size;
    char *format_string;
} tile_file_path_format_struct;

int tile_file(char *src_file_path, const tile_file_path_format_struct tile_file_path_format, const uint tile_size);
int save_tile(const char *cpc_dst_path, byte *p_dst_rows[], const int ci_src_color_type, const int ci_src_bit_depth, const unsigned int cui_dst_width, const unsigned int cui_dst_height);

void format_tile_path(char *buffer, tile_file_path_format_struct conf, uint x, uint y);

#endif
