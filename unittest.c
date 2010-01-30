#include <stdlib.h>
#include <stdio.h>

#include "colorproc.h"

int main(int argc, char** argv)
{
	int result = 0;
	const int xres = 100;
	const int yres = 10;
	unsigned char bitmap[xres*yres*3];

	int i;
	for (i=0; i<xres*yres*3;)
	{
		bitmap[i++] = 255;
		bitmap[i++] = 127;
		bitmap[i++] = 0;
	}

	int colors[5];
	printf("Getting colors\n");
	getcolors(colors, bitmap, xres, yres);

	for (i=0; i<5; ++i)
	{
		if (colors[i] != 0x00ff7f00)
		{
			printf("Wrong color at %d: %06x\n", i, colors[i]);
			result = 1;
		}
	}
	return result;
}
