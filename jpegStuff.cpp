#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>


int main(int argc, char* argv[])
{
unsigned char* raw_data = NULL;
int bytes_per_pixel = 3;   /* or 1 for GRACYSCALE images */
int color_space = JCS_RGB; /* or JCS_GRAYSCALE for grayscale images */

FILE *infile = fopen("testIMG.jpeg", "rb");

struct jpeg_decompress_struct cinfo;
struct jpeg_compress_struct cinfo2;
struct jpeg_error_mgr jerr;

JSAMPROW row_pointer[1];

cinfo.err = jpeg_std_error(&jerr);

jpeg_create_decompress(&cinfo);

//post setup

jpeg_stdio_src(&cinfo, infile);

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
fclose(infile);

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
fclose(outfile);

}
