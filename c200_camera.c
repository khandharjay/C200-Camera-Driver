#include <stdio.h>
#include <iostream>
#include "../libusb-1.0.9/libusb/libusb.h"
#include <malloc.h>
//#include <linux/types.h>
#include <usb.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/io.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define COLOR_CONVERT
#define HRES 320
#define VRES 240
#define HRES_STR "320"
#define VRES_STR "240"

#define MAX_IT 1
//#define LOCAL_PORT 54321
#define LOCAL_PORT_VPIPE 64321

#define VPIPE_OFF 

/*Phtread that streams the video over TCP*/
extern "C" void* vpipe_client(void*);

/*Number of frames to be captured*/
unsigned long framecnt=500;


char SaveBuffer[320*240*3];
char vpipeBuffer[320*240*3];


unsigned char bigbuffer[(1280*960)];

/*Header of the .ppm fine*/
char ppm_header[]="P6\n#9999999999 sec 9999999999 msec \n"HRES_STR" "VRES_STR"\n255\n";
char ppm_dumpname[]="test00000000.ppm";

sem_t mutex;


void broken_pipe_handler()

{
	printf("\n  broken pipe signal receiver \n");
}


/*Calcualtes the time difference between two timestamps*/
/*Used to calculate the time difference between capturing of two frames*/
timespec diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}


/*Helper function used to convert the image from YUV format to RGB */

void yuv2rgb(int y, int u, int v, unsigned char *r, unsigned char *g, unsigned char *b)
{
  
  int r1, g1, b1;
  
  // replaces floating point coefficients
  int c = y-16, d = u - 128, e = v - 128;       
  
  // Conversion that avoids floating point
  r1 = (298 * c           + 409 * e + 128) >> 8;
  g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
  b1 = (298 * c + 516 * d           + 128) >> 8;
  
  // Computed values may need clipping.
  if (r1 > 255) r1 = 255;
  if (g1 > 255) g1 = 255;
  if (b1 > 255) b1 = 255;
  
  if (r1 < 0) r1 = 0;
  if (g1 < 0) g1 = 0;
  if (b1 < 0) b1 = 0;
  
  *r = r1 ;
  *g = g1 ;
  *b = b1 ;
}


/*Write the image to a .ppm file*/
static void dump_ppm(const void *p, int size, unsigned int tag, struct timespec *time)
{
  int written, total, dumpfd;
  
  snprintf(&ppm_dumpname[4], 9, "%08d", tag);
  strncat(&ppm_dumpname[12], ".ppm", 5);
  dumpfd = open(ppm_dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);
  
  snprintf(&ppm_header[4], 11, "%010d", (int)time->tv_sec);
  strncat(&ppm_header[14], " sec ", 5);
  snprintf(&ppm_header[19], 11, "%010d", (int)((time->tv_nsec)/1000000));
  strncat(&ppm_header[29], " msec \n"HRES_STR" "VRES_STR"\n255\n", 19);
  written=write(dumpfd, ppm_header, sizeof(ppm_header));
  
  total=0;
  
  do
    {
      written=write(dumpfd, p, size);
      total+=written;
    } while(total < size);
  close(dumpfd);
  
}

struct timespec start;
struct timespec end;
struct timespec diff_time;
struct timespec total_time;
int entry = 0;
long double sum = 0;


static void process_image(const void *p, int size)
{
  int i, newi;
  struct timespec frame_time;
  int y_temp, y2_temp, u_temp, v_temp;
  unsigned char *pptr = (unsigned char *)p;
  
  
  // record when process was called
  clock_gettime(CLOCK_REALTIME, &frame_time);
  if(entry==0)
  {
	  start = frame_time;
	  entry = 1;
  }
  else
  {
	  end = frame_time;
	  diff_time=diff(start,end);
	  printf("time difference between two frames %ld\n",diff_time.tv_nsec);
	  start=end;
  }
  //printf("Dump YUYV converted to RGB size %d\n", size);
  
  // Pixels are YU and YV alternating, so YUYV which is 4 bytes
  // We want RGB, so RGBRGB which is 6 bytes
  //
  for(i=0, newi=0; i<size; i=i+4, newi=newi+6)
    {
      y_temp=(int)pptr[i]; u_temp=(int)pptr[i+1]; y2_temp=(int)pptr[i+2]; v_temp=(int)pptr[i+3];
      yuv2rgb(y_temp, u_temp, v_temp, &bigbuffer[newi], &bigbuffer[newi+1], &bigbuffer[newi+2]);
      yuv2rgb(y2_temp, u_temp, v_temp, &bigbuffer[newi+3], &bigbuffer[newi+4], &bigbuffer[newi+5]);
    }
  
#ifdef VPIPE_OFF  
  dump_ppm(bigbuffer, ((size*6)/4), framecnt, &frame_time);

#else
  sem_post(&mutex);

#endif
  
}




using namespace std;

void printdev(libusb_device *dev); //prototype of the function
FILE *outfile;
static int aborted = 0;
unsigned char* isotrans_buffer =(unsigned char*) malloc(30208);
unsigned char int_buf[16];

static void sighandler(int signum)
{
  printf("got signal %d", signum);
  aborted = 1;
}

/*
  static void interrupt_callback(struct libusb_transfer *transfer)
  {
  printf("interrupt_callback status %d\n", transfer->status);
  libusb_submit_transfer(transfer);
  }
*/

int mem_h = 0;
int end_of_image = 0;
unsigned char image_buffer[320*240*2];
int offset = 0;

/*This callback is called everytime a isochronous packet is received from the camera*/
/*The camera sends images using isochronous packets*/
static void capture_callback(struct libusb_transfer *transfer)
{
  
  int i;
/*One isoshrounous transfer involves 32 packets each of 944 bytes*/
  for (i = 0; i < 32; i++)
    {
      struct libusb_iso_packet_descriptor *desc = &transfer->iso_packet_desc[i];
      
	/*If the actual length of the packet was 12, we received a null packet in which the 12 bytes are just the header of that packet*/
	/*So we ignore packets which as just 12 bytes in length*/
	/*NOTE::The actual_length field denotes the number of bytes received in the packet*/
      if (desc->actual_length != 12)
	{
	/*The FID bit (the LSB bit of the second byte of the header) toggles at every frame boundary*/
	/*We monitor this bit, when the bit toggles we know that a complete frame has been received,and the new frame has started*/
	  if(mem_h != (transfer->buffer[(i*944)+1]&0x01))   
	    {
	      mem_h = transfer->buffer[(i*944)+1]&0x01;  /*Store the new value of the bit for further comparisions*/
	      end_of_image = 1;
	      offset = 0;                                /*Make the offset zero, to start storing from the start of the frame again*/
	      framecnt--;              
	      if(framecnt==0)
		aborted =1;
	      process_image(image_buffer,153600);
	     /*The image_buffer has been consumed by process_image*/
	     /*So again start filling the buffer from the start*/ 
	      int k;
	      for(k=0;k<(int)desc->actual_length;k++)
		{
		  if(k>11)
		    {
		      image_buffer[offset] = transfer->buffer[(i*944)+k];
		      offset++;
		    }
		}
	    }
	  else
	    {
	/*Keep storing the data received in each packet into the image_buffer*/
	      int k;
	      for(k=0;k<(int)desc->actual_length;k++)
		{
		  if(k>11)
		    {
		      image_buffer[offset] = transfer->buffer[(i*944)+k];
		      offset++;
		    }
		}
	    }
	  
	}
      if (desc->status != 0)
	{
	  printf("packet %d has status %d\n", i, desc->status);
	  continue;
	}
      
    }
  
/*Resubmit the isochronous transfer to keep on getting new data*/
  if(aborted !=1)
    libusb_submit_transfer(transfer);
}

//Allocate function for INterrupt trasfer
/*
  static struct libusb_transfer *allocate_int_transfer(libusb_device_handle *handle)
  {
  
  struct libusb_transfer *transfer = libusb_alloc_transfer(0);
  if (!transfer)
  printf("transfer alloc failure");
  libusb_fill_interrupt_transfer(transfer,handle,0x87, 
  int_buf, sizeof(int_buf),interrupt_callback, NULL, 0); 
  
  return transfer;
  }
*/

//Populating Transfer packet
static struct libusb_transfer *alloc_capture_transfer(libusb_device_handle *handle)
{
  struct libusb_transfer *transfer = libusb_alloc_transfer(32);
  int i;
  if (!transfer)
    printf("transfer alloc failure");
  transfer->dev_handle = handle;
  transfer->endpoint = 0x81;
  transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
  transfer->timeout = 5000;
  //transfer->buffer =(unsigned char*) malloc(30208);
  transfer->buffer = isotrans_buffer;
  transfer->length = 30208;          /*32 packets each of 944 bytes, 32 * 944 = 30208*/
  transfer->callback = capture_callback;
  transfer->num_iso_packets = 32;
  for (i = 0; i < 32; i++) {
    struct libusb_iso_packet_descriptor *desc =
      &transfer->iso_packet_desc[i];
    desc->length = 944;
  }
  return transfer;
  
}

pthread_t vpipe_thread;

int main()
{

    
  libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
  libusb_context *ctx = NULL; //a libusb session
  libusb_device_handle *dev_handle; //a device handle
  int r; //for return values
  ssize_t cnt; //holding number of devices in list
  sem_init(&mutex,0,1); 
  
  //initialize a library session
  r = libusb_init(&ctx);
  if(r < 0)
    {
      printf("Init Error\n");
      return 1;
    }
  //set verbosity level to 3, as suggested in the documentation
  libusb_set_debug(ctx, 3);
  //get the list of devices
  cnt = libusb_get_device_list(ctx, &devs);
  if(cnt < 0)
    {
      printf("Get Device Error\n");
    }
  
  //these are vendorID and productID I found for my usb device	
  dev_handle = libusb_open_device_with_vid_pid(ctx,0x46D,0x0802);
  if(dev_handle == NULL)
	{
	  printf("Cannot open device\n");
	  return 0;
	}
  else
    printf("Device Opened\n");

  
  //find out if kernel driver is attached
  if(libusb_kernel_driver_active(dev_handle, 0) == 1)
    {
      printf("Kernel Driver Active\n");
      if(libusb_detach_kernel_driver(dev_handle, 0) == 0)
	printf("Kernel Driver Detached!\n");
    }


  //claim interface 1 (Video streaming)
  r = libusb_claim_interface(dev_handle,1); 
  if(r!=0)
	  printf("Error claiming interface 1\n");

  //claim interface 0 (Video control)
  r = libusb_claim_interface(dev_handle,0); 
  if(r!=0)
	  printf("Error claiming interface 0\n");
  unsigned char data[26] = {0};
  

/*Populating the data to be sent for SET_CUR request*/
  *(__le16 *)&data[0] = (uint16_t)(0x01); //Supposed to be value of bmHint, unknown to us
  data[2] = 0x01; //Uncompressed
  data[3] = 0x04; //320x240
  *(__le32 *)&data[4]  = (uint32_t)(0x00051615);//ctrl->dwFrameInterval);



  /*
  //INTERRUPT TRANSFER

  struct libusb_transfer *int_transfer;
  int_transfer = allocate_int_transfer(dev_handle);

  r = libusb_submit_transfer(int_transfer);
  if (r < 0)
	  printf("interrupt submit fail %d\n", r);

  if (libusb_handle_events(ctx) < 0)
	  printf("Error with handle events\n");
  
  */
  //Setting the configuration
  r = libusb_control_transfer(dev_handle,
		  	      0x21,/*USB_DIR_OUT*/
		  	      0x01,/*USB_SET_CUR*/
			      0x01<<8,/*UVC_VS_PROBE_CONTROL*/
			      0x00<< 8 |0x01,
			      data,
			      26,
			      5000);
  if(r<0)
  {
	  printf("Error in control transfer\n");
	  return 0;
  }


/*The GET_CUR request will return some new data, which will help us selecint the correct alternate setting*/
  //Getting the configuration
  r = libusb_control_transfer(dev_handle,
		  	      0xA1,/*USB_DIR_OUT*/
		  	      0x81,/*USB_GET_CUR*/
			      0x01<<8,/*UVC_VS_PROBE_CONTROL*/
			      0x00<< 8 |0x01,
			      data,
			      26,
			      5000);
  if(r<0)
  {
	  printf("Error in control transfer\n");
	  return 0;
  }

  /*
  int i = 0;
  for(i=0;i<26;i++)
	  printf("%02x\t",data[i]);
  printf("\n");
  */
  
  /* The next step is to send a request to set the video commit probe*/
  r = libusb_control_transfer(dev_handle,
		  	      0x21,/*USB_DIR_OUT*/
		  	      0x01,/*USB_SET_CUR*/
			      0x200,/*UVC_VS_PROBE_CONTROL*/
			      0x00<< 8 |0x01,
			      data,
			      26,
			      5000);
  if(r<0)
  {
	  printf("Error in commit control\n");
	  return 0;
  }

  //Selecting interface and alternate setting
  r = libusb_set_interface_alt_setting(dev_handle,1, 6);
  if(r<0)
  {
	  printf("Error in setting interface and alt setting\n");
	  return 0;
  }
  else
  {
	  printf("Done setting alt setting and selecting interface\n");
  }


/*Initially send 5 packtets*/
  struct libusb_transfer *transfer1, *transfer2, *transfer3, *transfer4, *transfer5;
  transfer1 = alloc_capture_transfer(dev_handle);
  transfer2 = alloc_capture_transfer(dev_handle);
  transfer3 = alloc_capture_transfer(dev_handle);
  transfer4 = alloc_capture_transfer(dev_handle);
  transfer5 = alloc_capture_transfer(dev_handle);
  
#ifndef VPIPE_OFF 
  pthread_create(&vpipe_thread,NULL,vpipe_client,NULL);
#endif  
  r = libusb_submit_transfer(transfer1);
  if (r < 0)
	  printf("capture submit fail %d\n", r);
  r = libusb_submit_transfer(transfer2);
  if (r < 0)
	  printf("capture submit fail %d\n", r);
  r = libusb_submit_transfer(transfer3);
  if (r < 0)
	  printf("capture submit fail %d\n", r);
  r = libusb_submit_transfer(transfer4);
  if (r < 0)
	  printf("capture submit fail %d\n", r);
  r = libusb_submit_transfer(transfer5);
  if (r < 0)
	  printf("capture submit fail %d\n", r);
  


  //for(i=0;i<5;i++)
	 // libusb_handle_events(ctx);
  //Slecting interface and alternate setting
  /*
  r = libusb_set_interface_alt_setting(dev_handle,1, 0);
  if(r<0)
  {
	  printf("Error in setting interface and alt setting\n");
	  return 0;
  }
  else
  {
	  printf("Done setting alt setting and selecting interface\n");
  }

  */
  struct sigaction sigact;
  sigact.sa_handler = sighandler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(SIGINT, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGQUIT, &sigact, NULL);




  //Handle events

  /*
  while(!aborted)
 	  if(libusb_handle_events(ctx)<0)
   		  printf("Error handling events\n");	
  */
 
  while(!aborted)
  {
	  if(libusb_handle_events(ctx)<0)
		  break;
  }



  //sum =(float)sum/1000000000.0;
  //printf("Average difference between consecutive frames %f \n\n",sum);
  /*
  const struct libusb_pollfd **my_fd = libusb_get_pollfds(ctx);
  if(my_fd == NULL)
	  printf("Error on getting list of files desc\n");

  int count = 0;
  for (i=0; my_fd[i] != NULL; i++)
  {
	  count++;
	 // pollfd_array[i].fd = poll_usb[i]->fd;
	 // pollfd_array[i].events = poll_usb[i]->events;
  }
  printf("count is %d\n",count);

  */
  //Exit stuff
  //release the claimed interface


  //Selecting interface and alternate setting
  r = libusb_set_interface_alt_setting(dev_handle,1, 0);
  if(r<0)
  {
	  printf("Error in setting interface and alt setting\n");
	  return 0;
  }
  r = libusb_release_interface(dev_handle, 1);
  if(r!=0)
    {
      printf("Cannot Release Interface\n");
      return 1;
    }
  printf("Released Interface 1\n");
  r = libusb_release_interface(dev_handle, 0);
  if(r!=0)
    {
      printf("Cannot Release Interface\n");
      return 1;
    }
  printf("Released Interface 0\n");
 

  libusb_free_transfer(transfer1);	
  libusb_free_transfer(transfer2);	
  libusb_free_transfer(transfer3);	
  libusb_free_transfer(transfer4);	
  libusb_free_transfer(transfer5);	
  libusb_close(dev_handle); //close the device we opened
  libusb_free_device_list(devs, 1); //free the list, unref the devices in it
  libusb_exit(ctx); //close the session
  return 0;
}



//To print device configurations
void printdev(libusb_device *dev)
{
  libusb_device_descriptor desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0)
    {
      printf("Failed to get device descriptor\n");
      return;
    }
  printf("No. of possible configurations: %d\n",desc.bNumConfigurations);
  printf("Device Class: %x\n ",desc.bDeviceClass);
  printf("VendorID: %x\n",desc.idVendor);
  printf("ProductID: %x\n",desc.idProduct);
  libusb_config_descriptor *config;
  libusb_get_config_descriptor(dev, 0, &config);
  printf("Interfaces: %d",config->bNumInterfaces);
  const libusb_interface *inter;
  const libusb_interface_descriptor *interdesc;
  const libusb_endpoint_descriptor *epdesc;
  for(int i=0; i<(int)config->bNumInterfaces; i++)
    {
      printf("\n");
      inter = &config->interface[i];
      cout<<"Number of alternate settings: "<<inter->num_altsetting<<" | ";
      for(int j=0; j<inter->num_altsetting; j++)
	{
	  printf("\n");
	  interdesc = &inter->altsetting[j];
	  cout<<"Interface Number: "<<(int)interdesc->bInterfaceNumber<<" | ";
	  cout<<"Number of endpoints: "<<(int)interdesc->bNumEndpoints<<" | ";
	  for(int k=0; k<(int)interdesc->bNumEndpoints; k++)
	    {
	      printf("\n");
	      epdesc = &interdesc->endpoint[k];
	      cout<<"Descriptor Type: "<<(int)epdesc->bDescriptorType<<" | ";
	      printf("EP Address: %x",epdesc->bEndpointAddress);
			}
	}
    }
	cout<<endl<<endl<<endl;
	libusb_free_config_descriptor(config);
}

void* vpipe_client(void *ptr)           /*Send images to the host for display and debug*/
{
        FILE *fp;
	int i, client_sock/*,option,num_sets*/;
        /*struct hostent *hp;*/
        struct sockaddr_in client_sockaddr;
        struct linger opt;
        int sockarg;
        
       // int rec_val=0;
        const char *headerData[4]={
                        "P6\n",
                        "#test\n",
                        "320 240\n",
                        "255\n"
        };
       
	if(ptr)
    	{
	}
 
        if((client_sock=socket(AF_INET, SOCK_STREAM, 0)) < 0)
          {
                perror("client: socket");
                exit(-1);
          }

          client_sockaddr.sin_family = AF_INET;
          client_sockaddr.sin_port = htons(LOCAL_PORT_VPIPE);
          client_sockaddr.sin_addr.s_addr = inet_addr("172.21.74.227");

          /* discard undelivered data on closed socket */ 
          opt.l_onoff = 1;
          opt.l_linger = 0;

          sockarg = 1;

          setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*) &opt, sizeof(opt));

          setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockarg, sizeof(int));

          if(connect(client_sock, (struct sockaddr*)&client_sockaddr,
                 sizeof(client_sockaddr)) < 0) 
          {
                perror("client: connect");
                exit(-1);
          }

          signal(SIGPIPE, broken_pipe_handler);

          fp = fdopen(client_sock, "r");
          if(fp)
            {}
          //num_sets = MAX_IT;
          
          
        while(1)
        {
                //semTake(vpipeClientSem,WAIT_FOREVER);
		sem_wait(&mutex);

                for(i=0;i<(320*240*3);i++)
                {
                        vpipeBuffer[i]= bigbuffer[i];
                }
                for(i=0; i<4; i++)
                {
                        send(client_sock, headerData[i],strlen(headerData[i]),0);
                }

                
                {
                        send(client_sock, (char *)&(vpipeBuffer), sizeof(char)*320*240*3, 0);
                        
                }
                
        }
                
        
          close(client_sock);
        
          exit(0);
}


