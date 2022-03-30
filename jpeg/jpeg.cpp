
#include "jpeg.h"

#include <stdlib.h> //exit
#include <string.h> //memcpy

#ifdef WIN32
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

GLOBAL(void)
write_JPEG_file (char * filename, int quality, int image_width, int image_height, JSAMPLE* image_buffer, int* q_table_lum, int* q_table_chroma, bool downsample)
{

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;		/* target file */
    JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
    int row_stride;		/* physical row width in image buffer */
  
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);
  
    cinfo.image_width = image_width; 	/* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    if(q_table_lum && q_table_chroma) {
        int scale_factor = jpeg_quality_scaling (quality);
        if(q_table_lum != 0)
            jpeg_add_quant_table(&cinfo, 0, (unsigned int*)q_table_lum, scale_factor, (boolean)1);
        if(q_table_chroma != 0)
            jpeg_add_quant_table(&cinfo, 1, (unsigned int*)q_table_chroma, scale_factor, (boolean)1);
    }
    else {
        jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
    }

    if(!downsample) {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
    }
    cinfo.optimize_coding = TRUE;
  
    jpeg_start_compress(&cinfo, TRUE);
  
    row_stride = image_width * 3;	/* JSAMPLEs per row in image_buffer */
  
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
  
    fclose(outfile);
}

GLOBAL(unsigned long)
write_JPEG_memory(unsigned char* mem, unsigned long mem_size,
	                int quality, int image_width, int image_height, JSAMPLE* image_buffer, J_COLOR_SPACE color, 
                    int* q_table_lum, int* q_table_chroma, bool downsample)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	/* More stuff */
	JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
	int row_stride;		/* physical row width in image buffer */

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	jpeg_mem_dest(&cinfo, &mem, (unsigned long*)&mem_size);

    cinfo.image_width = image_width; 	/* image width and height, in pixels */
	cinfo.image_height = image_height;
	cinfo.input_components = 3;		/* # of color components per pixel */
	cinfo.in_color_space = color; 	/* colorspace of input image */
	
    jpeg_set_defaults(&cinfo);

	if (q_table_lum && q_table_chroma) {
		int scale_factor = jpeg_quality_scaling(quality);
		if (q_table_lum != 0)
			jpeg_add_quant_table(&cinfo, 0, (unsigned int*)q_table_lum, scale_factor, (boolean)1);
		if (q_table_chroma != 0)
			jpeg_add_quant_table(&cinfo, 1, (unsigned int*)q_table_chroma, scale_factor, (boolean)1);
	}
	else {
		jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
	}

	if (!downsample) {
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}
	cinfo.optimize_coding = TRUE;

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = image_width * 3;	/* JSAMPLEs per row in image_buffer */

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	return mem_size;
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}


GLOBAL(int)
read_JPEG_file(char * filename, bool ycbcr, JSAMPLE* image_buffer)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;

    FILE * infile;		/* source file */
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return 0;
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	jpeg_create_decompress(&cinfo);

	jpeg_stdio_src(&cinfo, infile);

	(void)jpeg_read_header(&cinfo, TRUE);
	if (ycbcr == true) {
		cinfo.out_color_space = JCS_YCbCr;
	}


	(void)jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	char c1, c2, c3;
	while (cinfo.output_scanline < cinfo.output_height) {
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);
		JSAMPLE* ptr = buffer[0];
		for (int i = 0; i < row_stride; i += 3) {
			if (ycbcr) {
				c1 = *ptr++;
				c2 = *ptr++;
				c3 = *ptr++;
			}
			else {
				c3 = *ptr++;
				c2 = *ptr++;
				c1 = *ptr++;
			}

			*image_buffer++ = c1;
			*image_buffer++ = c2;
			*image_buffer++ = c3;
		}
	}

	(void)jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(infile);

	return 1;
}

GLOBAL(int)
read_JPEG_memory(unsigned char * mem, unsigned long mem_size, bool ycbcr, JSAMPLE* image_buffer)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	/* More stuff */
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);

		return 0;
	}
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, mem, mem_size);
    (void)jpeg_read_header(&cinfo, TRUE);
	if (ycbcr == true) {
		cinfo.out_color_space = JCS_YCbCr;
	}

	(void)jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)
	((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);
		JSAMPLE* ptr = buffer[0];
		for (int i = 0; i < row_stride; i += 3) {
			*image_buffer++ = *ptr++;
			*image_buffer++ = *ptr++;
			*image_buffer++ = *ptr++;
		}
	}
	(void)jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

	return 1;
}


 
GLOBAL(int)
read_JPEG_header_memory(unsigned char * mem, unsigned long mem_size, JPEG_Info* info)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, mem, mem_size);
	(void)jpeg_read_header(&cinfo, TRUE);

    info->width = cinfo.image_width;
    info->height = cinfo.image_height;
    info->no_comp = cinfo.num_components;
	jpeg_destroy_decompress(&cinfo);

	return 1;
}

