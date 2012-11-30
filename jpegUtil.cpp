#include "jpegUtil.h"
#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>

struct jpegDataStruct
{
	jpeg_source_mgr pub;
	unsigned char* dat;
	int len;
};

//http://stackoverflow.com/questions/6327784/how-to-use-libjpeg-to-read-a-jpeg-from-a-stdistream
void init_source(j_decompress_ptr cinfo)
{
	jpegDataStruct* src = (jpegDataStruct*)(cinfo->src);
	//seek to 0 here
}

boolean fill_buffer (j_decompress_ptr cinfo)
{
	//Read to buffer
	jpegDataStruct* src = (jpegDataStruct*)(cinfo->src);
	src->pub.next_input_byte = src->dat;
	src->pub.bytes_in_buffer = src->len;//how many you could read

	//eof?
	return src->len ? FALSE : TRUE;
}

void skip(j_decompress_ptr cinfo, long count)
{
	// Seek by count bytes forward
	// Make sure you know how much you have cached and subtract that
	// set bytes_in_buffer and next_input_byte
}

void term(j_decompress_ptr cinfo)
{
	//Close the stream, can be nop
}

void make_stream(j_decompress_ptr cinfo, dataStruct* dat)
{
	jpegDataStruct* src;

	if(cinfo->src == NULL)
	{
		/* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)
		(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, 
		JPOOL_PERMANENT, sizeof(jpegDataStruct));
	}

	src = reinterpret_cast<jpegDataStruct*>(cinfo->src);
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_buffer;
	src->pub.skip_input_data = skip;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = term;
	src->dat = dat->data;
	src->len = dat->len;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */
}

void jpegUtil::alterImage(dataStruct* dat)
{
	unsigned char* raw_data = NULL;
	int bytes_per_pixel = 3;   /* or 1 for GRACYSCALE images */
	int color_space = JCS_RGB; /* or JCS_GRAYSCALE for grayscale images */

	//FILE *infile = fopen("testIMG.jpeg", "rb");

	struct jpeg_decompress_struct cinfo;
	struct jpeg_compress_struct cinfo2;
	struct jpeg_error_mgr jerr;

	JSAMPROW row_pointer[1];

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&cinfo);
	make_stream(&cinfo, dat);

	//post setup

	//jpeg_stdio_src(&cinfo, dat->data);

	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);

	unsigned char* raw_image = (unsigned char*) malloc(cinfo.output_width * cinfo.output_height * cinfo.num_components);

	row_pointer[0] = (unsigned char*)malloc(cinfo.output_width * cinfo.num_components);

	unsigned long location = 0;
	while(cinfo.output_scanline < cinfo.image_height)
	{
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
		for(int i = 0; i < cinfo.image_width * cinfo.num_components; i++)
			raw_image[location++] = location % 3 != 0 ? row_pointer[0][i] : 0;
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	//free(row_pointer[0]);
	/*/fclose(infile);

	FILE *outfile = fopen("output.jpg", "wb");

	jpeg_create_compress(&cinfo2);
	jpeg_stdio_dest(&cinfo2, outfile);

	cinfo2.err = jpeg_std_error(&jerr);
	cinfo2.image_width = cinfo.image_width;
	cinfo2.image_height = cinfo.image_height;
	cinfo2.input_components = bytes_per_pixel;
	cinfo2.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo2);
	jpeg_start_compress(&cinfo2, TRUE);

	while(cinfo2.next_scanline < cinfo2.image_height)
	{
			row_pointer[0] = &raw_image[ cinfo2.next_scanline * cinfo2.image_width *  cinfo2.input_components];
			jpeg_write_scanlines( &cinfo2, row_pointer, 1 );
	}

	jpeg_finish_compress(&cinfo2);
	jpeg_destroy_compress(&cinfo2);
	fclose(outfile);*/
}
