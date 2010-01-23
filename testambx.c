/*
 * testlibusb.c
 *
 *  Test suite program
 */

#include <stdio.h>
#include <usb.h>

#include "ambxlib.h"


static int add_time(char* data, unsigned int time)
{
    data[0] = time / 0x100; //time in ms, msb
    data[1] = time % 0x100; //time in ms, lsb
    return 2;
}

static int add_rgb(char *data, char r, char g, char b)
{
    data[0] = r;
    data[1] = g;
    data[2] = b;
    return 3;
}

static int handleEnumDevice(DeviceInfo *info, void* context)
{
    printf("Device %d is available\n", info->id);
}

int main(int argc, char *argv[])
{
    int id = 0;
    int i, nr_controllers;
    if (ambx_init() < 0)
    {
	printf("ambx_init failed.\n");
        return -1;
    }
    if (ambx_open(id) < 0)
    {
        printf("ambx_open(%d) failed.\n", id);
        return -1;
    }
    
    int color = 0xff;
    ambx_set_light(id, 0, 0xff0080); //left
    sleep(1);
    ambx_set_light(id, 1, 0xff0000); //right
    sleep(1);
    ambx_set_light(id, 2, 0x00ff00); //l w
    sleep(1);
    ambx_set_light(id, 3, 0x0000ff); //m w
    sleep(1);
    ambx_set_light(id, 4, 0x80ff00); //r w
    sleep(1);

    for (i=0; i<5; ++i)
    {
        ambx_set_light(id, i, 0x000000);
    }
    /*Command 0xc
    r, g, b start
    r, g, b, increment
    s time step
    */

    unsigned int colors[8];
    colors[0] = 0xff0000;
    colors[1] = 0x00ff00;
    colors[2] = 0x0000ff;
    colors[3] = 0x808080;
    colors[4] = 0x808000;
    colors[5] = 0x008080;
    colors[6] = 0x800080;
    colors[7] = 0x808080;

    for (i=0; i<5; ++i)
	{
	    ambx_set_light_sequence(id, i, 1000, colors, 8);
	}

    ambx_close(id);
    return 0;
}

