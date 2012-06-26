#include <stdio.h>
#include "filehelper.h" 

int hexFromFile(const char* filename)
{
	FILE* fd = fopen(filename, "r");
	if (!fd) return -1;
	int result = -1;
	fscanf(fd, "%x", &result);
	fclose(fd);
	return result;
}

int intFromFile(const char* filename)
{
	FILE* fd = fopen(filename, "r");
	if (!fd) return -1;
	int result = -1;
	fscanf(fd, "%d", &result);
	fclose(fd);
	return result;
}

