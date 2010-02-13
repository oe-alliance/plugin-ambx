#include <stdlib.h>
#include <stdio.h>

#include "colorproc.h"
#include "filehelper.h"

static int testcolorproc()
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

#define ASSERTEQUAL(a,b) \
	if (a != b) { printf("Fail:" #a " != " #b ".\n"); return 1; }

static int testfilehelper()
{
	const char* testfile = "tmp_tst.tmp";
	unlink(testfile);

	// Error return value is -1
	ASSERTEQUAL(hexFromFile(testfile), -1);
	ASSERTEQUAL(intFromFile(testfile), -1);

	// 42
	FILE* f = fopen(testfile, "w");
	fprintf(f, "42\n");
	fclose(f);

	ASSERTEQUAL(hexFromFile(testfile), 0x42);
	ASSERTEQUAL(intFromFile(testfile), 42);

	unlink(testfile);
	return 0;
}

int main(int argc, char** argv)
{
	int result = 0;
	result |= testcolorproc();
	result |= testfilehelper();
	return result;
}
