/*********************************************************************************/
/*Standalone Linux Driver for the Logitech C200 camera -This is a UVC Webcam     */
/*Authors:John Pratt,Brijen Raval,Jay Khandhar                                   */
/*Spring 2013 - University of Colorado at Boulder                                */

/*A major part of the code has been developed using the UVC Specification 1.5    */
/*It can be found on http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_5.zip*/


/*We used the LibUSB 1.0 library to perform transfer on the USB bus, so that should be installed before running this code*/

/*The part of this code which converts the image from YUV2 to RGB is taken from Dr.Sam Siewert's capture.c code*/
/*We also used usbmon trace facility to check what data is passed to the camera on the USB bus by Sam's capture.c code and tried to send the same packets from our code*/


/*The camera has two interfaces
Interface number 0 = Video Control
Endpoint Number    = 0x87 (Interrupt Endpoint)

Interface number 1 = Video Streaming
Endpoint Number    = 0x81 (Isochronous Endpoint)

The Video Control Interface is used to select the format and resolution at which we want to stream the image, using control packets
The Video Streaming Interface is used stream the images from the camera, using isochrounous packets

This code streams 30 fps. These are uncompressed frames at 320x240 resolution

We found the initialization procedure for the camera,in the USB_Video_FAQ_1.5.pdf, which is in the .zip file in the above link
The procedure is described on page 36 of the FAQ pdf, under the title "Stream Format Negotiation (setup for streaming)"
The steps are as follows
1.Set the MaxPayloadTransferSize fields to zero (These values will be returned from the device)
2.Prepare the Data Structure to be sent to the camera- Set the FormatIndex, FrameIndex and FrameInterval Fields to the desired values.
  Other fields can be safely set to zero in the above structure
3.Send a SET_CUR  probe control packet to the camera, with the above data structure(its size is 26 bytes)
4.Send a GET_CUR  probe control packet to the camera, the camera will now return a packet of 26 bytes, 
  In this 26 bytes returned MaxPayLoadTransferSize field returned by the camera is important to us
  This is the value specific to the frame and format that we have selected
5.Send a SET_CUR  commit control packet to the camera, using the data values returned by the camera in the above steps.
6.Now attempt to select the alt setting based on the MaxPayloadTransferSize field returned by the camera.
  For 320x240 Uncompressed the Alt Setting Number to be selected is 6.

After doing this we are now ready to start to start streaming frames from the camera.
For the Alternate Setting that we have selected has the wMaxPacketSize of 944 bytes, so each isochrounous packet would be of 944 bytes.
NOTE: The streaming endpoint is a asynchronous endpoint. Hence we submit requests for isochronous packets, and the camera will send us frames when it is ready with some data.



Initially we allocate 5 isochronous transfers each of 30208 bytes each

1 Isochronous transfer= 32 Isochronous packets of 944 bytes each
So size of one isochronous transfer is of 32 x 944 bytes=30208 bytes.

We send 5 such transfers at the very start,and then whenever the camera sends us back one transfer with the image data, we resubmit this transfer to the camera, to receive the next isochronous transfer.

In the call back, for each isochronous packet we check the actual length of the packet received. 
Initially the camera sends isochronous packets with no data, they just have the 12 byte header
We discard these initial null packets.

The 12 byte header in each isochronous packet is defined by the UVC spec in the file USB_Video_Payload_Uncompressed_1.5.pdf
See section 2.4 titled "Stream Header" on page number 8 of that pdf.

For YUV2 each pixel is of 2 bytes, so for a 320x240 image, the size would be 320 x 240 x 2 = 153600 bytes.

We use the FID (Frame Identifier) bit to check when we have received a complete frame.
The FID bit toggles when one frame ends and another begins. We monitor this bit and stop capturing one frame when this bit toggles.

Now although the isochronous packet can send upto 944 bytes, it does not always send 944 bytes. So we check the actual_length field of the iso packet received and then transfer that many bytes of data to our image_buffer. Roughly for each iso packet we receive upto 620 bytes of data.

When the FID toggles, a new frame has started, so we take the buffer of 153600 bytes and convert it to RGB format.

NOTE:The UVC driver in Linux, also sends and receives packets from the control endpoint, but we found it to be redundant.
*/



