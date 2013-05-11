/*********************************************************************************/
/*Standalone Linux Driver for the Logitech C200 camera -This is a UVC Webcam     */
/*Authors:Brijen Raval,John Pratt,Jay Khandhar                                   */
/*Spring 2013 - University of Colorado at Boulder                                */

/*A major part of the code has been developed using the UVC Specification 1.5    */
/*It can be found on http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_5.zip*/


/*The camera has two interfaces
Interface number 0 = Video Control
Endpoint Number    = 0x87 (Interrupt Endpoint)

Interface number 1 = Video Streaming


The Video Control Interface is used to select the format and resolution at which we want to stream the image, using control packets
The Video Streaming Interface is used stream the images from the camera, using isochrounous packets

This code streams 30 fps. These are uncompressed frames at 320x240 resolution

We found the initialization procedure for the camera, the USB_Video_FAQ_1.5.pdf, which is in the .zip file in the above link
The procedure is described on page 36 of the FAQ pdf, under the title "Stream Format Negotiation (setup for streaming)"
The steps are as follows
1.Set the MaxPayloadTransferSize fields to zero (These values will be returned from the device)
2.Prepare the Data Structure to be sent to the camera- Set the FormatIndex, FrameIndex and FrameInterval Fields to the desired values.
  Other fields can be safely set to zero in the above structure/*********************************************************************************/
/*Standalone Linux Driver for the Logitech C200 camera -This is a UVC Webcam     */
/*Authors:Brijen Raval,John Pratt,Jay Khandhar                                   */
/*Spring 2013 - University of Colorado at Boulder                                */

/*A major part of the code has been developed using the UVC Specification 1.5    */
/*It can be found on http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_5.zip*/


/*The camera has two interfaces
Interface number 0 = Video Control
Endpoint Number    = 0x87 (Interrupt Endpoint)

Interface number 1 = Video Streaming


The Video Control Interface is used to select the format and resolution at which we want to stream the image, using control packets
The Video Streaming Interface is used stream the images from the camera, using isochrounous packets

This code streams 30 fps. These are uncompressed frames at 320x240 resolution

We found the initialization procedure for the camera, the USB_Video_FAQ_1.5.pdf, which is in the .zip file in the above link
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

After doing this we are now ready to start to start streaming frames from the c
3.Send a SET_CUR  probe control packet to the camera, with the above data structure(its size is 26 bytes)
4.Send a GET_CUR  probe control packet to the camera, the camera will now return a packet of 26 bytes, 
  In this 26 bytes returned MaxPayLoadTransferSize field returned by the camera is important to us
  This is the value specific to the frame and format that we have selected
5.Send a SET_CUR  commit control packet to the camera, using the data values returned by the camera in the above steps.
6.Now attempt to select the alt setting based on the MaxPayloadTransferSize field returned by the camera.
  For 320x240 Uncompressed the Alt Setting Number to be selected is 6.

After doing this we are now ready to start to start streaming frames from the camera.
