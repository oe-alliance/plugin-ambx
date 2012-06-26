#include <stdio.h>
#include <string.h>
#include <usb.h>

#include "ambxlib.h"

/*
Raw message format
byte 0: sequence number and some sort of command classifier
byte 1: destination id and command id
byte 2: length of data (including this field)
byte 3 and more: command specific data
*/

#define AMBX_USB_TIMEOUT 200
/* only support MAX_CONTROLLERS = 16 ambx_controllers */
#define MAX_CONTROLLERS 16
typedef struct {
        struct usb_device  *controller;
        usb_dev_handle     *handle;
        int                 claimed_interface;
        int                 config_value;
        int                 alt_setting;
} ambx_controller_info;

static ambx_controller_info controllers[MAX_CONTROLLERS];
static int nr_of_controllers = 0;

typedef int (*DeviceFoundCallback)(int id, DeviceInfo *info);
typedef int (*EnumControllerCallback)(struct usb_device* dev, void* context);


static int ambx_init_controller(int id);
static int ambx_enumerate_controllers(struct usb_bus *usb_busses, EnumControllerCallback callback, void* context);
static int get_number_of_devices(usb_dev_handle *handle);
static int get_device_info(usb_dev_handle *handle, int id, EnumDeviceCallback callback, void *context);
//static int ambx_enumerate_devices(int id);
static int handleControllerFound(struct usb_device* newDevice, void* context);
static int start_transactions(struct usb_dev_handle *handle);
static int check_controller(int id);
static int send_and_wait_for_response(usb_dev_handle *handle, char* send_buffer, int send_count, char* receive_buffer, int* receive_count);
static char get_msg_cmd_byte(char cmd);

int ambx_init()
{
    int i;
    struct usb_bus *busses;
    for (i=0; i<MAX_CONTROLLERS; ++i)
    {
        controllers[i].controller = 0;
        controllers[i].handle = 0;
        controllers[i].claimed_interface = -1;
        controllers[i].config_value = -1;
        controllers[i].alt_setting = -1;
    }
    usb_init();
    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();
    ambx_enumerate_controllers(usb_busses, handleControllerFound, 0);
}


int ambx_nr_controllers()
{
    return nr_of_controllers;
}

int ambx_controller_get_status(int id)
{
    int error = check_controller(id);
    if (error < 0)
        return error;

    int status = 0;
    if (controllers[id].handle)
        status |= AMBX_OPEN;

    if (controllers[id].claimed_interface >= 0)
        status |= AMBX_IFACE_CLAIMED;

    return status;
}


int ambx_open(int id)
{
    int error = check_controller(id);
    if (error < 0)
        return error;

    if (controllers[id].handle != 0)
        return -E_ALREADY_OPEN;

    controllers[id].handle = usb_open(controllers[id].controller);
    if (controllers[id].handle == 0)
        return -E_OPEN_FAILED;

    if (ambx_init_controller(id) < 0)
    {
        usb_close(controllers[id].handle);
        controllers[id].handle = 0;

        return -E_INIT_CONTROLLER_FAILED;
    }

    start_transactions(controllers[id].handle);

    //ambx_enumerate_devices(id);

    return SUCCESS;
}

int ambx_close(int id)
{
    int error = check_controller(id);
    if (error < 0)
        return error;

    if (controllers[id].handle == 0)
        return -E_CLOSED;

    if (controllers[id].claimed_interface >=0)
        usb_release_interface(controllers[id].handle, controllers[id].claimed_interface);

    usb_close(controllers[id].handle);
    controllers[id].handle = 0;
    controllers[id].claimed_interface = -1;
    controllers[id].config_value = -1;
    controllers[id].alt_setting = -1;
}

int ambx_enumerate_devices(int controller, EnumDeviceCallback cb, void* context)
{
    int i;
    int error = check_controller(controller);
    if (error < 0)
        return error;

    usb_dev_handle *handle = controllers[controller].handle;
    if (handle == 0)
        return -E_CLOSED;

    int nr_of_devices = get_number_of_devices(controllers[controller].handle);

    int check_next = 1;
    for (i=0; i<nr_of_devices && check_next; i++)
    {
        check_next = get_device_info(controllers[controller].handle, i, cb, context);
    }
}

//void set_fan(usb_dev_handle *handle, int id, int speed, unsigned char x = 0x01, unsigned char cmd = 0x0b);
int ambx_set_fan(int controller, int id, int speed, unsigned char x, unsigned char cmd)
{
    char message[4];
/*    char response[10];
    int respCount = sizeof(response);
*/

    usb_dev_handle *handle = controllers[controller].handle;
    if (handle == 0)
        return -E_CLOSED;

    message[0] = get_msg_cmd_byte(x);
    message[1] = (id << 4) | cmd;
    message[2] = 1;
    message[3] = speed;
    //send_and_wait_for_response(handle, message, sizeof(message), response, &respCount);
    return usb_interrupt_write(handle, 0x02, message, sizeof(message), AMBX_USB_TIMEOUT);
}
static int add_rgb(char *data, unsigned int color)
{
    data[0] = (color & 0xff0000) >> 16;
    data[1] = (color & 0x00ff00) >> 8;
    data[2] = (color & 0x0000ff);
    return 3;
}

static int add_time(char *data, short time)
{
    data[0] = time / 0x100;
    data[1] = time % 0x100;
    return 2;
}

int ambx_set_light(int controller, int id, unsigned int color)
//int ambx_set_light(int controller, int id, char r, char g, char b)
{
    char message[6];
    usb_dev_handle *handle = controllers[controller].handle;
    if (handle == 0)
        return -E_CLOSED;

    message[0] = get_msg_cmd_byte(0x01);
    message[1] = (id << 4) | 0x0b;
    message[2] = 0x03; //length
    add_rgb(&message[3], color);

    return usb_interrupt_write(handle, 0x02, message, sizeof(message), AMBX_USB_TIMEOUT);
}

int ambx_set_light_sequence(int controller, int id, short time, unsigned int *colors, int num_of_colors)
{
    char message[0x40];
    int offset;
    int i;
    usb_dev_handle *handle = controllers[controller].handle;
    if (handle == 0)
        return -E_CLOSED;
    if (num_of_colors > 20)
        return -E_INVALID_ARGUMENT;

    message[0] = get_msg_cmd_byte(0x01);
    message[1] = (id << 4) | 0x0b;

    offset = 3;
    offset += add_time(&message[offset], time);
    for (i=0; i<num_of_colors; ++i)
    {
        offset += add_rgb(&message[offset], colors[i]);
    }
    message[2] = (1 << 6) | ((offset-3) & 0x3f); //length
    return usb_interrupt_write(handle, 0x02, message, offset, AMBX_USB_TIMEOUT);
}

int ambx_send_raw(int controller, int id, int cmd, char* data, unsigned char length)
{
    char message[0x40];
    int receive_count = 0x40;
    usb_dev_handle *handle = controllers[controller].handle;
    if (handle == 0)
        return -E_CLOSED;
    
    if (length > 0x40-3)
        return -E_INVALID_ARGUMENT;

    message[0] = get_msg_cmd_byte(0x01);
    message[1] = (id <<4) | cmd;

    memcpy(&message[2], data, length);
    //hex_dump("Send raw", message, length + 2);
    return send_and_wait_for_response(handle, message, length + 2, message, &receive_count);
}

static char shared_buffer[64];
static char get_msg_cmd_byte(char cmd)
{
    static char msg_nr = 1;
    return (((msg_nr++ % 0x10) << 4) | cmd);
}

static int send_and_wait_for_response(usb_dev_handle *handle, char* send_buffer, int send_count, char* receive_buffer, int* receive_count)
{
    int received = 0;
    //hex_dump("Writing", send_buffer, send_count);
    int sent = usb_interrupt_write(handle, 0x02, send_buffer, send_count, AMBX_USB_TIMEOUT);
    if (sent == send_count)
    {
        received = usb_interrupt_read(handle, 0x81, receive_buffer, *receive_count, AMBX_USB_TIMEOUT);
        if (received > 0)
        {
            //hex_dump("Received", receive_buffer, received > 2 ? receive_buffer[2] + 3 : received);
        }
        else
        {
            printf("Error reading data %d\n", received);
        }
    }
    else
    {
        printf("Not all bytes sent\n");
    }
    if (sent == send_count && received >= 0)
    {
        *receive_count = received;
        return 1; //true
    }
    return 0; //false
}

static int check_controller(int id)
{
    if (id < 0 || id > MAX_CONTROLLERS)
        return -E_INVALID_CONTROLLER_ID;

    if (!controllers[id].controller)
        return -E_CONTROLLER_NOT_FOUND;

    return SUCCESS;
}

static int start_transactions(usb_dev_handle *handle)
{
    char message[0x40];

    if (usb_interrupt_read(handle, 0x81, message, 0x20, AMBX_USB_TIMEOUT) >= 0)
    {
        //hex_dump("Pending data 0x81", message, message[2]+3);
    }

    if (usb_interrupt_read(handle, 0x83, message, 0x40, AMBX_USB_TIMEOUT) >= 0)
    {
        //hex_dump("Pending data 0x83", message, message[2]+3);
    }

    message[0] = 0x00;
    message[1] = 0x00;
    int tralala = sizeof(message);
    if (send_and_wait_for_response(handle, message, 1, message, &tralala))
    {
        printf("success, received count=%d, data length=%d\n", tralala, message[2]);
        //hex_dump("Start transactions", message, message[2]+3);
        //return 1;
    }
    else
    {
        printf("No response on start transactions\n");
    }

    //????
    message[0] = get_msg_cmd_byte(0x01);
    message[1] = 0xF0;  //destination F, command 0
    message[2] = 0x01;  //length 1
    message[3] = 0x00;
    int received = 64;
    if (send_and_wait_for_response(handle, message, 4, message, &received))
    {
        printf("Received %d bytes in response\n", received);
        //hex_dump("Init received", message, message[2]+3);
        printf("init done\n");
    }

    return 0;
}

static int get_number_of_devices(usb_dev_handle *handle)
{
    int error = -1;
    int nr_of_devs = 0;
    int i=0;

    int received = 64;
    int retryCount = 0;
    do 
    {
        shared_buffer[0] = get_msg_cmd_byte(0x01);
        shared_buffer[1] = 0xF0;    //get number of devices
        shared_buffer[2] = 0x01;
        shared_buffer[3] = 0x00;
        if (send_and_wait_for_response(handle, shared_buffer, 4, shared_buffer, &received))
        {
            int message_length = shared_buffer[2];
            for (i=0; i<message_length; ++i)
            {
                nr_of_devs += shared_buffer[3+i] << ((message_length-i-1)*8);
            }
        }
    } while (nr_of_devs == 0 && ++retryCount < 2);
    return nr_of_devs;
}

static void reset_device(usb_dev_handle *handle, int id)
{
    char message[4];
    message[0] = get_msg_cmd_byte(0x01);
    message[1] = (id << 4) | 0x00;
    message[2] = 0x01;
    message[3] = 0x0F;

    char response[64];
    int response_size = 64;
    send_and_wait_for_response(handle, message, 4, response, &response_size);
}

static void get_capabilities(const char *data, int len, DeviceInfo *info)
{
    int data_len = data[1];
    int index = 4;
    int i=0;

    int position = data[0x17];
    int type = data[0x21];

    const char* txt= "Unknown";
    switch (position)
    {
        case 2: 
            txt = "Left"; 
            info->position = Left;
            break;
        case 3: 
            txt = "Middle"; 
            info->position = Middle;
            break;
        case 4: 
            txt = "Right"; 
            info->position = Right;
            break;
    }
    printf("Reported position: %s\n", txt);

    txt = "Unknown";
    switch (type)
    {
        case 1: 
            txt = "Fan"; 
            info->type = Fan;
            break;
        case 2: 
            txt = "Rumbler"; 
            info->type = Rumbler;
            break;
        case 3: 
            txt = "Light"; 
            info->type = RGBLight;
            break;
    }
    printf("Reported type: %s\n", txt);
    printf("Description: %s\n", data+0x30);
}


static int get_device_info(usb_dev_handle *handle, int id, EnumDeviceCallback callback, void *context)
{
    int error = -1;
    int retryCount = 0;
    char message[64];
    message[0] = get_msg_cmd_byte(0x01);
    message[1] = (id << 4) | 0x01;  //command
    message[2] = 0x03;  //length (databytes)
    message[3] = 0x00;  //
    message[4] = 0x00;  //start offset
    message[5] = 0x3B;  //received buffer size (excluding header: msg nr, cmd, len bytes)

    char message_data[255];
    char received_data[128];
    int received_count = 64;
    int message_len = 0;
    
    printf("Requesting device info for device %d\n", id);

    //hex_dump("Sending", message, message[2]+3);
    if (send_and_wait_for_response(handle, message, 6, received_data, &received_count))
    {
        int i=0;
        for (i=5; i<received_data[2] + 3; ++i)
        {
            message_data[i-5] = received_data[i];
        }
        message_len = i-5;
        //printf("message len: %d\n", message_len);
        //check received_data
        int required_length = received_data[6];
        if (required_length > 0x3B-2)
        {
            message[0] = get_msg_cmd_byte(0x01);
            message[4] = 0x3B;  //start offset

            received_count = 64;
            if (send_and_wait_for_response(handle, message, 6, received_data+64, &received_count))
            {
                for (i=5; i<received_data[64+2] + 3; ++i)
                {
                    message_data[message_len + i-5] = received_data[64+i];
                }
                message_len += (i - 5);
            }
        }


        //hex_dump("Actual data", message_data, message_len);
        message_data[ 0x30 + message_data[0x2f]] = 0;
        DeviceInfo info;
        memset(&info, 0, sizeof(DeviceInfo));
        info.id = id;
        get_capabilities(message_data, message_len, &info);
        return callback(&info, context);
    }
    return 1;
}


typedef struct tag_devices {
    unsigned int vendorId;
    unsigned int productId;
} devices_t;

/* Keep this table sorted on vendorId */
devices_t devices[] = 
{
    { 0x0471, 0x083f },
};


/* Call with open device */
static int ambx_init_controller(int id)
{
    int iface = 0;
    int altSetting = 0;
    int configValue = 0;
    usb_dev_handle *handle = controllers[id].handle;
    struct usb_device *dev = controllers[id].controller;

    configValue = dev->config[0].bConfigurationValue;
    if (usb_set_configuration(handle, configValue) < 0)
    {
        printf("Error setting configuration %d\n", dev->config[0].bConfigurationValue);
        return -E_USB_CONFIG;
    }

    /* Only check first interface and alternate settings */
    if (dev->config[0].bNumInterfaces > 0 &&
        dev->config[0].interface[0].num_altsetting > 0 )
    {
        iface = dev->config[0].interface[0].altsetting[0].bInterfaceNumber;
        altSetting = dev->config[0].interface[0].altsetting[0].bAlternateSetting;

        printf("Claiming interface %d\n", iface);
        if (usb_claim_interface(handle, iface) < 0)
        {
            printf("Error claiming interface...\n");
            return -E_USB_CLAIM_IFACE;
        }
        if (usb_set_altinterface(handle, altSetting) < 0)
        {
            printf("Error setting active alternate setting %d\n", altSetting);
            usb_release_interface(handle, iface);
            return -E_USB_CONFIG;
        }
    }

    controllers[id].claimed_interface = iface;
    controllers[id].alt_setting = altSetting;
    controllers[id].config_value = configValue;
    return SUCCESS;
}

static int handleControllerFound(struct usb_device* newDevice, void* context)
{
    int i;
    if (newDevice == 0)
        return 0;   /* something went wrong, stop looking for devices */

    printf("Found supported controller\n");
    for (i = 0; i < newDevice->descriptor.bNumConfigurations; i++)
    {
        printf("Configuration %d\n", i);
        // print_configuration(&newDevice->config[i]);
    }

    controllers[nr_of_controllers ].handle = 0;
    controllers[nr_of_controllers ].controller = newDevice;
    nr_of_controllers++;

    return (nr_of_controllers < MAX_CONTROLLERS);
}



static int ambx_enumerate_controllers(struct usb_bus *usb_busses, EnumControllerCallback callback, void* context)
{
    int nrFound = 0;
    int x = 0;
    int findNext = 1;

    struct usb_bus *bus;
    for (bus = usb_busses; bus && findNext; bus = bus->next)
    {
        struct usb_device *dev;
        for (dev = bus->devices; dev && findNext; dev = dev->next)
        {
            for (x=0; x<sizeof(devices)/sizeof(*devices) && findNext; ++x)
            {
                if (dev->descriptor.idVendor == devices[x].vendorId &&
                    dev->descriptor.idProduct == devices[x].productId )
                {
                    findNext = callback(dev, context);
                    nrFound++;
                }
            }
        }
    }
    return nrFound;
}

// static int ambx_enumerate_devices(int id)
// {
//     int i;
//     get_device_info(controllers[id].handle, 0x0f);
//     int nr_of_devices = get_number_of_devices(controllers[id].handle);
// 
//     for (i=0; i<nr_of_devices; i++)
//     {
//         get_device_info(controllers[id].handle, i);
//     }
//     return 0;
// }

