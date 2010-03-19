#ifndef __AMBXLIB_H__
#define __AMBXLIB_H__


#define SUCCESS                     0
#define E_INVALID_CONTROLLER_ID     1
#define E_CONTROLLER_NOT_FOUND      2
#define E_ALREADY_OPEN              3
#define E_OPEN_FAILED               4
#define E_USB_CONFIG                5
#define E_USB_CLAIM_IFACE           6
#define E_CLOSED                    7
#define E_INIT_CONTROLLER_FAILED    8
#define E_INVALID_ARGUMENT          9

/* Status codes for ambx_controller_get_status() */
#define AMBX_OPEN                   1
#define AMBX_IFACE_CLAIMED          2

typedef enum {
    RGBLight,
    Fan,
    Rumbler,
} DeviceType;

typedef enum {
    Left,
    Middle,
    Right,
    Center,
    Everywhere,
} Position;

typedef struct {
    int id;
    DeviceType type;
    Position position;
} DeviceInfo;


typedef int (*EnumDeviceCallback)(DeviceInfo *info, void *context);
/*
typedef int (*EnumControllerCallback)(struct usb_device* dev, void* context);
*/

int ambx_init(void);
/*
int ambx_enumerate_devices(DeviceFoundCallback *callback);
*/

int ambx_nr_controllers(void);
int ambx_controller_get_status(int controller);
int ambx_open(int controller);
int ambx_close(int controller);
int ambx_enumerate_devices(int controller, EnumDeviceCallback cb, void* context);

int ambx_set_light(int controller, int id, unsigned int color);
int ambx_set_light_sequence(int controller, int id, short time, unsigned int *colors, int num_of_colors);
int ambx_set_fan(int controller, int id, int speed, unsigned char x, unsigned char cmd);

int ambx_send_raw(int controller, int id, int cmd, char* data, unsigned char length);

        
#endif
