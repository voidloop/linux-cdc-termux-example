/*
 * This is a simple example to communicate with a CDC-ACM USB device
 * using libusb.
 *
 * Author: Christophe Augier <christophe.augier@gmail.com>
 * https://github.com/tytouf/libusb-cdc-example/blob/master/cdc_example.c
 * 
 * Update: 2022/08/06
 * 
 * I updated this example to read data from a Raspberry Pi Pico 
 * connected via USB to my Android phone (on Termux).
 * https://wiki.termux.com/wiki/Termux-usb
 * 
 * Marco Esposito Micenin <marco.esposito@gmail.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>

#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02

libusb_device_handle *devh;

/* The Endpoint address are hard coded. You should use lsusb -v to find
 * the values corresponding to your device.
 */
static int ep_in_addr  = 0x82;
static int ep_out_addr = 0x02;

void write_char(unsigned char c)
{
    /* To send a char to the device simply initiate a bulk_transfer to the
     * Endpoint with address ep_out_addr.
     */
    int actual_length;
    if (libusb_bulk_transfer(devh, ep_out_addr, &c, 1,
                             &actual_length, 0) < 0) {
        fprintf(stderr, "Error while sending char\n");
    }
}

int read_chars(unsigned char * data, int size)
{
    /* To receive characters from the device initiate a bulk_transfer to the
     * Endpoint with address ep_in_addr.
     */
    int actual_length;
    int rc = libusb_bulk_transfer(devh, ep_in_addr, data, size, &actual_length,
                                  1000);
    if (rc == LIBUSB_ERROR_TIMEOUT) {
        printf("timeout (%d)\n", actual_length);
        return -1;
    } else if (rc < 0) {
        fprintf(stderr, "Error while waiting for char\n");
        return -1;
    }

    return actual_length;
}

int main(int argc, char **argv) {
    libusb_context *context;

    struct libusb_device_descriptor desc;
    unsigned char buffer[256];
    int fd;
    int rc;
    
    if ((argc < 1) || (sscanf(argv[1], "%d", &fd) != 1)) {
        printf("usage: %s <fd>", argv[0]);
        exit(1);
    }

    rc = libusb_set_option(NULL, LIBUSB_OPTION_WEAK_AUTHORITY);
    if (rc < 0) {
        fprintf(stderr, "Error setting libusb option: %s\n",
                libusb_error_name(rc));
        exit(1);
    }

    rc = libusb_init(&context);
    if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n",
                libusb_error_name(rc));
        exit(1);
    }

    rc = libusb_wrap_sys_device(context, (intptr_t) fd, &devh);
    if (rc < 0) {
        fprintf(stderr, "Error wrapping file descriptor: %s\n",
                libusb_error_name(rc));
        goto out;
    }
    
    /* As we are dealing with a CDC-ACM device, it's highly probable that
     * Linux already attached the cdc-acm driver to this device.
     * We need to detach the drivers from all the USB interfaces. The CDC-ACM
     * Class defines two interfaces: the Control interface and the
     * Data interface.
     */
    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            libusb_detach_kernel_driver(devh, if_num);
        }
        rc = libusb_claim_interface(devh, if_num);
        if (rc < 0) {
            fprintf(stderr, "Error claiming interface: %s\n",
                    libusb_error_name(rc));
            goto out;
        }
    }
	
	/* Start configuring the device:
	 * - set line state
	 */
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
                                 0, NULL, 0, 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
        goto out;
    }

    /* - set line encoding: here 115200 8N1
     * 115200 = 0x01C200 ~> 0x00, 0xC2, 0x01 in little endian
     */
    unsigned char encoding[] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding,
                                 sizeof(encoding), 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
        goto out;
    }

    /* We can now start sending or receiving data to the device
     */
    unsigned char buf[65];
    int len;
    
    while(1) {
        write_char('t');
        len = read_chars(buf, 64);
        buf[len] = 0;
        fprintf(stdout, "Received: \"%s\"\n", buf);
        sleep(1);
    }

    libusb_release_interface(devh, 0);

out:
    if (devh) {
        libusb_close(devh);
    }
    libusb_exit(context);
    return rc;

}
