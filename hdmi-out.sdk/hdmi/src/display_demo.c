/************************************************************************/
/*																		*/
/*	display_demo.c	--	ZYBO Display demonstration 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Bobrowicz												*/
/*	Copyright 2016, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		HDMI output capabilities on the ZYBO. It is a good	            */
/*		example of how to properly use the display_ctrl drivers.	    */
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		2/5/2016(SamB): Created											*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "display_demo.h"
#include "display_ctrl/display_ctrl.h"
#include <stdio.h>
#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "timer_ps/timer_ps.h"
#include "xparameters.h"
#include "font.h"
#include "xadcps.h"
#include "xstatus.h"
#include "PmodENC.h"
#include "sleep.h"
/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_IRPT_ID XPS_FPGA3_INT_ID
#define VID_GPIO_IRPT_ID XPS_FPGA4_INT_ID
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR
#define XADC_DEVICE_ID 		XPAR_XADCPS_0_DEVICE_ID
/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

/*
 * Display Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;

/*
 * Framebuffers for video data
 */
u8  frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__((aligned(0x20)));
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers
u8 bkgndFrame [DEMO_MAX_FRAME];

/*
 * XADC driver instance
 */
static XAdcPs XAdcInst;

#define FULL_SCALE	0
#define AMPLIFIED	1
#define FAST_SAMPLING	0
#define SLOW_SAMPLING	1
#define BTN_PRESSED		1
#define BTN_NPRESSED	0
#define VERTICAL_AXIS_WIDTH	76
PmodENC myDevice;
int verticalScale;
int horizontalScale;

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

int main(void)
{
	DemoInitialize();
	OscilloscopeRun();

	return 0;
}


void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	XAdcPs_Config *ConfigPtr;
	XAdcPs *XAdcInstPtr = &XAdcInst;
	int i;

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}

	/*
	 * Initialize a timer used for a simple delay
	 */
	TimerInitialize(SCU_TIMER_ID);

	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_1);


	/*
	 * Initialize the XAdc driver.
	 */
	ConfigPtr = XAdcPs_LookupConfig(XADC_DEVICE_ID);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start ADC driver demo initialization%d\r\n", Status);
		return;
	}
	XAdcPs_CfgInitialize(XAdcInstPtr, ConfigPtr, ConfigPtr->BaseAddress);

	/*
	 * Self Test the XADC/ADC device
	 */
	Status = XAdcPs_SelfTest(XAdcInstPtr);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start ADC device during demo initialization%d\r\n", Status);
		return;
	}

	xil_printf("All initialization ok\r\n");
	return;
}

void OscilloscopeRun(){


	int nextFrame = 0;
	int i;
	int adcVal;
	unsigned int previous;
	u32 state;

	unsigned int samples[1920-VERTICAL_AXIS_WIDTH];
	unsigned int *samplesPtr = samples;


	DisplayStop(&dispCtrl);
	DisplaySetMode(&dispCtrl, &VMODE_1920x1080);
	DisplayStart(&dispCtrl);
	ENC_begin(&myDevice, XPAR_PMODENC_0_AXI_LITE_GPIO_BASEADDR);

	paintBkgnd(NULL, dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride);


	while(1){
		nextFrame = (dispCtrl.curFrame == 0) ? 1 : 0;
		memcpy(dispCtrl.framePtr[nextFrame], bkgndFrame, DEMO_MAX_FRAME);
		state = ENC_getState(&myDevice);
		check_RotEnc(state, &verticalScale, &horizontalScale);

		// Record first half of samples before trigger
		for(i=0;i<(1920-VERTICAL_AXIS_WIDTH)/2;i++){
			if(horizontalScale == FAST_SAMPLING)
				usleep(7);
			else
				usleep(500);

			samplesPtr++;
			if(samplesPtr >= &samples[1920])
				samplesPtr = samples;

			*samplesPtr = XAdcPolled();
		}

		// Wait for trigger
		do{
			if(horizontalScale == FAST_SAMPLING)
				usleep(7);
			else
				usleep(500);

			previous = *samplesPtr;
			samplesPtr++;
			if(samplesPtr >= &samples[1920])
				samplesPtr = samples;

			*samplesPtr = XAdcPolled();


		}while(!((previous < 32768) && (*samplesPtr >= 32768)));


		// Record second half of samples after trigger
		for(i=0;i<(1920-VERTICAL_AXIS_WIDTH)/2;i++){
			if(horizontalScale == FAST_SAMPLING)
				usleep(7);
			else
				usleep(500);

			samplesPtr++;
			if(samplesPtr >= &samples[1920])
				samplesPtr = samples;

			*samplesPtr = XAdcPolled();
		}

		// There is a weird offset in the circular buffer of VERTICAL_AXIS_WIDTH
		for(i = 0; i<VERTICAL_AXIS_WIDTH ; i++){
			samplesPtr++;
			if(samplesPtr >= &samples[1920])
				samplesPtr = samples;
		}

		// Print samples
		for(i=VERTICAL_AXIS_WIDTH;i<1920;i++){
			samplesPtr++;
			if(samplesPtr >= &samples[1920])
				samplesPtr = samples;

			if(verticalScale == FULL_SCALE){
				adcVal = ((*samplesPtr)>>6) & 0x3FF;
			}
			else{
				if((*samplesPtr) >= 40960)
					adcVal = 1024;
				else if((*samplesPtr) > 24576)
					adcVal = ((((*samplesPtr)>>4)-(2048-512))) & 0x3FF;
				else
					adcVal = 0;
			}
			printPixel(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, i, 1024-adcVal, 0xFFFF00);
		}


		printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "  0"  ,    0, 500,  0xFFFF00);

		if(horizontalScale == SLOW_SAMPLING){
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "50ms/div" , 0, 1047, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.05" ,   159, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.25" ,   559, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.5" , 1051, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.75" , 1551, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.9" , 1851, 1036, 0xFFFF00);

		}
		else{
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, " 1ms/div" , 0, 1047, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "1" ,   159, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "5" ,   559, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "10" , 1051, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "15" , 1551, 1036, 0xFFFF00);
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "18" , 1851, 1036, 0xFFFF00);

		}

		if(verticalScale == FULL_SCALE){
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "0.1V/div" , 0, 1025, 0xFFFF00);
		}
		else{
			printText(dispCtrl.framePtr[nextFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, "25mV/div" , 0, 1025, 0xFFFF00);
		}



		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) dispCtrl.framePtr[nextFrame], DEMO_MAX_FRAME);
		DisplayChangeFrame(&dispCtrl, nextFrame);
	}

}


void printColor(u8 *frame, u32 width, u32 height, u32 stride, u32 color){

	u8 red   = (color&0xFF0000)>>16;
	u8 green = (color&0x00FF00)>>8;
	u8 blue  = (color&0x0000FF);
	u32 iPixelAddr;
	u32 xcoi, ycoi;



	for(xcoi = 0; xcoi < (width*4); xcoi+=4){
		iPixelAddr = xcoi;
		for(ycoi = 0; ycoi < height; ycoi++){

			frame[iPixelAddr] = red;
			frame[iPixelAddr + 1] = blue;
			frame[iPixelAddr + 2] = green;


			/*
			 * This pattern is printed one vertical line at a time, so the address must be incremented
			 * by the stride instead of just 1.
			 */
			iPixelAddr += stride;
		}
	}
	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);


}


// Assuming 1080p
void paintBkgnd(u8 *frame, u32 width, u32 height, u32 stride){

	u8 red, green, blue;
	u32 iPixelAddr;
	u32 xcoi, ycoi;

	for(xcoi = 0; xcoi < (width*4); xcoi+=4){
		iPixelAddr = xcoi;
		for(ycoi = 0; ycoi < height; ycoi++){

			// Edges and x axis
			if(((xcoi == 75*4)&&(ycoi < 1024)) || ((xcoi > 65*4)&&((ycoi == 1024)||(ycoi == 512)))){
				red = 0xFF;
				green = 0xFF;
				blue = 0xFF;
			}
			// Grid
			else if (((xcoi > 65*4)&&(ycoi < 1034)) && (((xcoi-75*4)%(100*4) == 0)||(ycoi%100 == 12))){
				red = 0x40;
				green = 0x40;
				blue = 0x40;
			}
			// Background
			else{
				red = 0;
				green = 0;
				blue = 0;
			}


			bkgndFrame[iPixelAddr] = blue;
			bkgndFrame[iPixelAddr + 1] = green;
			bkgndFrame[iPixelAddr + 2] = red;

			/*
			 * This pattern is printed one vertical line at a time, so the address must be incremented
			 * by the stride instead of just 1.
			 */
			iPixelAddr += stride;
		}
	}
}

void printText(u8 *frame, u32 width, u32 height, u32 stride, char* string, u32 xStart, u32 yStart, u32 color){

	u32 i = 0;

	while(*string != '\0'){
		printLetter(frame, width, height, stride, *string, xStart+i*FONT_BIG_WIDTH, yStart, color);
		i++;
		string++;
	}
}

void printLetter(u8 *frame, u32 width, u32 height, u32 stride, unsigned char letter, u32 xStart, u32 yStart, u32 color){

	u8 red   = (color&0xFF0000)>>16;
	u8 green = (color&0x00FF00)>>8;
	u8 blue  = (color&0x0000FF);
	u32 iPixelAddr;
	u32 xcoi, ycoi;
	u8 i,j;

	// Check we dont go out of range
	if(xStart+FONT_BIG_WIDTH >= width)
		return;
	if(yStart+FONT_BIG_HEIGHT >= height)
		return;


	i = 0;
	for(xcoi = xStart*4; xcoi < (xStart+FONT_BIG_WIDTH)*4; xcoi+=4){
		iPixelAddr = xcoi+yStart*stride;
		j = 0;
		for(ycoi = yStart; ycoi < yStart+FONT_BIG_HEIGHT; ycoi++){

			if(font_big[letter-0x20][j] & (0x8000>>i)){
				frame[iPixelAddr] = blue;
				frame[iPixelAddr + 1] = green;
				frame[iPixelAddr + 2] = red;
			}


			/*
			 * This pattern is printed one vertical line at a time, so the address must be incremented
			 * by the stride instead of just 1.
			 */
			iPixelAddr += stride;
			j++;
		}
		i++;
	}

}


void printPixel(u8 *frame, u32 width, u32 height, u32 stride, u32 xStart, u32 yStart, u32 color){

	u8 red   = (color&0xFF0000)>>16;
	u8 green = (color&0x00FF00)>>8;
	u8 blue  = (color&0x0000FF);
	u32 iPixelAddr;

	iPixelAddr = xStart*4 + yStart*stride;
	frame[iPixelAddr] = blue;
	frame[iPixelAddr + 1] = green;
	frame[iPixelAddr + 2] = red;

}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi;
	u32 iPixelAddr;
	u8 wRed, wBlue, wGreen;
	u32 wCurrentInt;
	double fRed, fBlue, fGreen, fColor;
	u32 xLeft, xMid, xRight, xInt;
	u32 yMid, yInt;
	double xInc, yInc;


	switch (pattern)
	{
	case DEMO_PATTERN_0:

		xInt = width / 4; //Four intervals, each with width/4 pixels
		xLeft = xInt * 3;
		xMid = xInt * 2 * 3;
		xRight = xInt * 3 * 3;
		xInc = 256.0 / ((double) xInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		yInt = height / 2; //Two intervals, each with width/2 lines
		yMid = yInt;
		yInc = 256.0 / ((double) yInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		fBlue = 0.0;
		fRed = 256.0;
		for(xcoi = 0; xcoi < (width*4); xcoi+=4)
		{
			/*
			 * Convert color intensities to integers < 256, and trim values >=256
			 */
			wRed = (fRed >= 256.0) ? 255 : ((u8) fRed);
			wBlue = (fBlue >= 256.0) ? 255 : ((u8) fBlue);
			iPixelAddr = xcoi;
			fGreen = 0.0;
			for(ycoi = 0; ycoi < height; ycoi++)
			{

				wGreen = (fGreen >= 256.0) ? 255 : ((u8) fGreen);
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				if (ycoi < yMid)
				{
					fGreen += yInc;
				}
				else
				{
					fGreen -= yInc;
				}

				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			if (xcoi < xLeft)
			{
				fBlue = 0.0;
				fRed -= xInc;
			}
			else if (xcoi < xMid)
			{
				fBlue += xInc;
				fRed += xInc;
			}
			else if (xcoi < xRight)
			{
				fBlue -= xInc;
				fRed -= xInc;
			}
			else
			{
				fBlue += xInc;
				fRed = 0;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	case DEMO_PATTERN_1:

		xInt = width / 7; //Seven intervals, each with width/7 pixels
		xInc = 256.0 / ((double) xInt); //256 color intensities per interval. Notice that overflow is handled for this pattern.

		fColor = 0.0;
		wCurrentInt = 1;
		for(xcoi = 0; xcoi < (width*4); xcoi+=4)
		{

			/*
			 * Just draw white in the last partial interval (when width is not divisible by 7)
			 */
			if (wCurrentInt > 7)
			{
				wRed = 255;
				wBlue = 255;
				wGreen = 255;
			}
			else
			{
				if (wCurrentInt & 0b001)
					wRed = (u8) fColor;
				else
					wRed = 0;

				if (wCurrentInt & 0b010)
					wBlue = (u8) fColor;
				else
					wBlue = 0;

				if (wCurrentInt & 0b100)
					wGreen = (u8) fColor;
				else
					wGreen = 0;
			}

			iPixelAddr = xcoi;

			for(ycoi = 0; ycoi < height; ycoi++)
			{
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			fColor += xInc;
			if (fColor >= 256.0)
			{
				fColor = 0.0;
				wCurrentInt++;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	default :
		xil_printf("Error: invalid pattern passed to DemoPrintTest");
	}
}


int XAdcPolled()
{
	u16 VccPdroRawData;
	XAdcPs *XAdcInstPtr = &XAdcInst;


	/*
	 * Read the AD14 input Voltage Data from the
	 * ADC data registers.
	 */
	VccPdroRawData = XAdcPs_GetAdcData(XAdcInstPtr, XADCPS_CH_AUX_MIN+14);

	// Sign extension and right shift, so the reading is the 10 LSB bits
	//data = (VccPdroRawData>>6) + (VccPdroRawData & 0x8000 ? 0xFC : 0);

	return VccPdroRawData;
}


void check_RotEnc(u32 state, int *verticalScalePtr, int *horizontalScalePtr){
	static int prevBtn = 0;
	int currentBtn = 0;

	if(ENC_switchOn(state)){
		*verticalScalePtr = AMPLIFIED;
	}
	else{
		*verticalScalePtr = FULL_SCALE;
	}

	currentBtn = ENC_buttonPressed(state);
	if(currentBtn != BTN_NPRESSED && prevBtn == BTN_NPRESSED){
		*horizontalScalePtr = (*horizontalScalePtr == FAST_SAMPLING) ? SLOW_SAMPLING: FAST_SAMPLING;
	}

	prevBtn = currentBtn;
}