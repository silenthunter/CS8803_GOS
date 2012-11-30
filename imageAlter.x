struct dataStruct
{
	int len;
	unsigned char* data;
};

program IMAGEALTERPROG
{
	version IMAGEALTERVERS
	{
		dataStruct IMAGEALTER(dataStruct) = 1;
	} = 1;
} = 0x2007001;
