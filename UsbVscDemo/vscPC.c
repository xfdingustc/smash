// TODO: WIP, not fully compatible

#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include "stdbool.h"

#define CAMERALOG(fmt, args...) \
    do { \
        printf("%s() line %d "fmt"\n", \
            __FUNCTION__, __LINE__, ##args); \
    } while (0);

#define BUFFER_SIZE 512*1024
#define PACKET_SIZE 512
#define DEV_INTERFACE 0
#define DEV_VID 0x040E
#define DEV_PID 0xf63B

int g_out_end_point = 0, g_in_end_point = 0;
#define VENDORID      0x040e
#define IS_TO_DEVICE  0 /* to device */
#define IS_TO_HOST    0x80  /* to host */
#define IS_BULK       2

#define EP1 1
#define EP2 2
#define EP3 3

#define REPEAT_COUNT  1
#define TRANSFER_SIZE 512*1024
#define TIMEOUT_MS    6000
#define TRANSFER_EP EP1
uint8_t input_data[BUFFER_SIZE];
uint8_t output_data[BUFFER_SIZE];

FILE* file;
static char* file_name;

unsigned int endpointNo;



libusb_device_handle *dev_handle;

static int GetEndPoint(libusb_device *dev) {
  struct libusb_config_descriptor *config;
  int r = libusb_get_active_config_descriptor(dev, &config);
  if (r < 0) {
    return -1;
  }

  int iface_idx;
  for (iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) {
    const struct libusb_interface *iface = &config->interface[iface_idx];
    int altsetting_idx;

    for (altsetting_idx = 0; altsetting_idx < iface->num_altsetting; altsetting_idx++) {
      const struct libusb_interface_descriptor *altsetting = &iface->altsetting[altsetting_idx];
      int ep_idx;

      for (ep_idx = 0; ep_idx < altsetting->bNumEndpoints; ep_idx++) {
        const struct libusb_endpoint_descriptor *ep = &altsetting->endpoint[ep_idx];

        if (IS_TO_DEVICE == (ep->bEndpointAddress & 0x80)&& IS_BULK == (ep->bmAttributes & 0x03)) {
          g_out_end_point = ep->bEndpointAddress;
          printf("outEndPoint:[%x]\r\n", g_out_end_point);
        }
        if (IS_TO_HOST == (ep->bEndpointAddress & 0x80) && IS_BULK == (ep->bmAttributes & 0x03)) {
          g_in_end_point = ep->bEndpointAddress;
          printf("inEndPoint:[%x]\r\n", g_in_end_point);
        }
      }
    }

  }

  libusb_free_config_descriptor(config);

  return 0;
}

static void print_devs(libusb_device **devs) {
  libusb_device *dev;
  int i = 0, j = 0;
  uint8_t path[8];

  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
      fprintf(stderr, "failed to get device descriptor");
      return;
    }

    printf("%04x:%04x (bus %d, device %d)", desc.idVendor, desc.idProduct,
    libusb_get_bus_number(dev), libusb_get_device_address(dev));

    r = libusb_get_port_numbers(dev, path, sizeof(path));
    if (r > 0) {
      printf(" path: %d\n", path[0]);
      for (j = 1; j < r; j++)
        printf(".%d", path[j]);
      }
    GetEndPoint(dev);
    printf("\n");
  }
}

static bool checkArgs(int argc, char** argv) {
  if (argc != 2) {
    return false;
  }

  return true;
}

void * Thread1(void* lpParam) {
  int r;
  int actual;
  int k;
  FILE* output_file;

  CAMERALOG("Thread1 running, waiting to receive data");

  for(k = 0; k < REPEAT_COUNT; k ++) {
    output_file = fopen("/castle/ouput.jpeg", "wb");
    if (!output_file) {
        CAMERALOG("######### open /castle/ouput.jpeg failed");
    }

    CAMERALOG("######## before receiving data");
    r = libusb_bulk_transfer(dev_handle, (TRANSFER_EP | LIBUSB_ENDPOINT_IN), input_data,
        TRANSFER_SIZE, &actual, TIMEOUT_MS);
    CAMERALOG("######## after receiving data");

    if (output_file) {
        fwrite(input_data, 1, BUFFER_SIZE, output_file);
        fclose(output_file);
    }

    if (LIBUSB_ERROR_TIMEOUT == r) {
      r = 0;
      CAMERALOG("timeout\n");
    } else if (r) {
      CAMERALOG("int_xfer=%d\n", r);
    }
    if (actual > 0) {
      CAMERALOG("Read %d bytes\n", actual);
    }
  }

  CAMERALOG("Thread1 exit");

  pthread_exit(0);
}

void * Thread2(void* lpParam) {
  int r;
  int actual;
  int k;

  CAMERALOG("Thread2 running, begin sending data to device");

  for(k = 0; k < REPEAT_COUNT; k ++) {
    if (file) {
        fseek(file, 0, SEEK_END);
        int file_length = ftell(file);
        CAMERALOG("file_length = %d", file_length);
        fseek(file, 0, SEEK_SET);
        fread(output_data, 1, file_length, file);
    } else {
        CAMERALOG("the image is not opened!");
    }

    
    int send_len = 16*1024;
    CAMERALOG("######## before sending data %d bytes", send_len);
    r = libusb_bulk_transfer(dev_handle, (TRANSFER_EP | LIBUSB_ENDPOINT_OUT), output_data,
        TRANSFER_SIZE, &actual, TIMEOUT_MS);
    CAMERALOG("######## after sending data");

    if (LIBUSB_ERROR_TIMEOUT == r) {
      r = 0;
      CAMERALOG("timeout\n");
    } else if (r) {
      CAMERALOG("int_xfer=%d\n", r);
    }
    if (actual > 0) {
      CAMERALOG("Wrote %d bytes\n", actual);
    }
  }

  CAMERALOG("Thread2 exit");

  pthread_exit(0);
}

int main(int argc, char** argv) {
    printf("#########");

  pthread_t thread1, thread2;
  libusb_device **devs;
  libusb_context *context = NULL;
  int r;
  ssize_t cnt;

  printf("#########");

  if (!checkArgs(argc, argv)) {
    printf("Usage: vscPC input\n");
    return -1;
  }

  file_name = argv[1];
  file = fopen(file_name, "rb");
  if (file == NULL) {
    CAMERALOG("Cannot open file %s\n", file_name);
    return -1;
  } else {
    CAMERALOG("open image %s successfully", file_name);
  }

  pthread_attr_t attr;

  r = libusb_init(NULL);
  if (r < 0) {
    return r;
  }

  cnt = libusb_get_device_list(NULL, &devs);
  if (cnt < 0) {
    return (int) cnt;
  }

  print_devs(devs);
  libusb_free_device_list(devs, 1);

  dev_handle = libusb_open_device_with_vid_pid(context, DEV_VID, DEV_PID);

  if (dev_handle == NULL) {
    printf("Cannot open device\n");
    return 1;
  } else {
    printf("device opened\n");
  }

  r = libusb_claim_interface(dev_handle, DEV_INTERFACE);
  if (r < 0) {
    printf("error code: %d\n", r);
    printf("Cannot claim interface\n");
    return 1;
  }

  if (pthread_attr_init(&attr) != 0) {
    printf("pthread_attr_init error");
  }

  if (pthread_create(&thread1, &attr, &Thread1, 0) != 0) {
    printf("Thread1 creation failed\n");
  } else {
    printf("Thread1 created\n");
  }

  if (pthread_create(&thread2, &attr, &Thread2, 0) != 0) {
    printf("Thread2 creation failed\n");
  } else {
    printf("Thread2 created\n");
  }

  pthread_join(thread1, NULL);

  pthread_join(thread2, NULL);

  r = libusb_release_interface(dev_handle, DEV_INTERFACE);
  if (r != 0) {
    printf("Cannot release interface\n");
    return 1;
  }
  printf("Interface released\n");

  libusb_close(dev_handle);

  libusb_exit(context);

  CAMERALOG("Mission completed!");
  return 0;
}
