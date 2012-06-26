#include <stdlib.h>
#include <stdio.h>

#include "colorproc.h"
#include "filehelper.h"
#include "Fader.h"

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

static int testcolorproc2()
{
	unsigned int result[256] = {0};
	unsigned char bitmap[5*4] =
		{
			1, 2, 3, 4, 42,
			1, 2, 3, 4, 42, 
			5, 5, 5, 5, 42,
			0, 0, 9, 9, 42
		};
	histogram2(bitmap, 0, 4, 5, 4, result);
	int i;
	ASSERTEQUAL(result[0], 1);
	ASSERTEQUAL(result[1], 2);
	ASSERTEQUAL(result[2], 0);
	ASSERTEQUAL(result[5], 2);
	ASSERTEQUAL(result[42], 0);
	return 0;	
}


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

static int testfader()
{
        Fader f;
	fader_init(&f, 3);

        f.target[0] = 0;
        f.target[1] = 128;
        f.target[2] = 255;

        fader_commit(&f, 0, 0);
        fader_update(&f, 0);

        ASSERTEQUAL(f.current[0], 0);
        ASSERTEQUAL(f.current[1], 128);
        ASSERTEQUAL(f.current[2], 255);
        f.target[0] = 128;
        f.target[1] = 255;
        f.target[2] = 0;

        fader_commit(&f, 0, 1000);

        fader_update(&f, 500);

        ASSERTEQUAL(f.current[0], 64);
        ASSERTEQUAL(f.current[1], 191);
        ASSERTEQUAL(f.current[2], 127);

        fader_commit(&f, 1000, 2000);
        fader_update(&f, 1000);

        ASSERTEQUAL(f.current[1], 191);

        fader_update(&f, 1500);

        ASSERTEQUAL(f.current[0], 96);
        ASSERTEQUAL(f.current[1], 223);
        ASSERTEQUAL(f.current[2], 63);

        fader_update(&f, 2000);

        ASSERTEQUAL(f.current[0], 128);
        ASSERTEQUAL(f.current[1],255);
        ASSERTEQUAL(f.current[2], 0);

        return 0;
}

int main(int argc, char** argv)
{
	int result = 0;
	result |= testcolorproc();
	result |= testcolorproc2();
	result |= testfilehelper();
	result |= testfader();
	return result;
}
