#ifndef __HEADER_TILE_H
#define __HEADER_TILE_H

#include "util.h"

int tile_file(char *src_path, const char *dst_path_format, const uint TILE_SIZE);
int save_tile(const char *cpc_dst_path, byte *p_dst_rows[], const int ci_src_color_type, const int ci_src_bit_depth, const unsigned int cui_dst_width, const unsigned int cui_dst_height);

#endif
