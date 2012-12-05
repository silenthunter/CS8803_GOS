//#include "jpegUtil.h"
#include "imageAlter.h"
#include <stdio.h>
#include <iostream>

using namespace std;

int main(int argc, char* argv[])
{
	FILE *infile = fopen("testIMG.jpeg", "rb");

	long fSize = 0;

	fseek(infile, 0, SEEK_END);
	fSize = ftell(infile);
	//rewind(infile);
	fseek(infile, 0, SEEK_SET);

	dataStruct dat;
	dataStruct* result;
	dat.len = fSize;
	dat.data = (unsigned char*) malloc(fSize);

	fread(dat.data, 1, fSize, infile);
	fclose(infile);

	CLIENT* clnt = clnt_create("localhost", IMAGEALTERPROG, IMAGEALTERVERS, "tcp");
	if (clnt == NULL) {
		clnt_pcreateerror ("localhost");
		exit (1);
	}
	//jpegUtil::alterImage(&dat);
	cout << dat.len << endl;
	result = imagealter_1(&dat, clnt);
	if(result == NULL)
		clnt_perror(clnt, "call failed");
	clnt_destroy(clnt);

	FILE *outfile = fopen("output.jpg", "wb");

	fwrite(result->data, 1, result->len, outfile);
	fclose(outfile);
}
