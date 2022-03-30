#pragma once

#include <stdio.h>
#include <setjmp.h>

#if defined(_MSC_VER)
#pragma comment(lib, "turbojpeg-static.lib")
#endif

extern "C" {
#include "jpeg/include/jpeglib.h"
// #include "transupp.h"            /* Support routines for jpegtran */
}

GLOBAL(void)
write_JPEG_file (char * filename, int quality, int image_width, int image_height, JSAMPLE* image_buffer, int* qtable_lum=0, int* qtable_chroma=0,bool downsample=true);

GLOBAL(unsigned long)
write_JPEG_memory(unsigned char * mem, unsigned long mem_size,
    int quality, int image_width, int image_height, JSAMPLE* image_buffer, J_COLOR_SPACE color,
    int* qtable_lum = 0, int* qtable_chroma = 0, bool downsample = true);

GLOBAL(int)
read_JPEG_file(char * filename, bool ycbcr, JSAMPLE* image_buffer);

GLOBAL(int)
read_JPEG_memory(unsigned char * mem, unsigned long mem_size, bool ycbcr, JSAMPLE* image_buffer);




typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int no_comp;
	
} JPEG_Info;

typedef struct {
	struct jpeg_decompress_struct srcinfo;
	jvirt_barray_ptr* src_coef_arrays;
	// jpeg_transform_info transformoption;
} HANDLE_JPEG;


GLOBAL(int)
read_JPEG_header_memory(unsigned char * mem, unsigned long mem_size, JPEG_Info* info);
