#include <XBD_HAL.h>
#include <XBD_FRW.h>
#include <XBD_debug.h>

#include <string.h>
/* device specific includes */


#include "inc/hw_ints.h"
#include "inc/hw_flash.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/watchdog.h"
#include "driverlib/uart.h"
#include "drivers/osram96x16x1.h"




/* your functions / global variables here */
#define STACK_CANARY (inv_sc?0x3A:0xC5)

void writeByte(unsigned char byte);

void XBD_init()
{
  /* inititalisation code, called once */
      //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_6MHZ);
    OSRAM96x16x1Init(false);

	/* enable uart0 for debug output */
   //
    // Enable the peripherals used by this example.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Set GPIO A0 and A1 as UART pins.
    //
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Configure the UART for 115200, 8-N-1 operation.
    //
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));

   /* enable uart1 for xbd<-> xbh comm */

    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_2 | GPIO_PIN_3);


    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 250000,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));
    
    writeByte('A');

	/* Set PORTD Pin 0 as Output for XBH timing acquisition */
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0);
 	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, 1);
                 

}


inline void XBD_sendExecutionStartSignal()
{
  /* code for output pin = low here */
 	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, 0);

}

inline void XBD_sendExecutionCompleteSignal()
{
  /* code for output pin = high here */
 	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, 1);
}


void XBD_debugOut(char *message)
{
  /* if you have some kind of debug interface, write message to it */
  // OSRAM96x16x1StringDraw(message, 0, 0);


   char *m=message;
	while(*m) UARTCharPut(UART0_BASE, *m++);

   
}


/* read a byte from the serial port */
unsigned char readByte()
{
	unsigned char  byte= UARTCharGet(UART1_BASE);

  	return byte;
}


/* write a byte to the serial port */
void writeByte(unsigned char byte)
{
	UARTCharPut(UART1_BASE, byte);
}


#include <XBD_streamcomm.i>
/*
* XBD_streamcomm.i
* Provides emulation of TWI (I▓C) over stream oriented channel (e.g. UART).
* May be included by HALs that want to use it, otherwise unused.
* Requires the HAL to provide
*
* unsigned char readByte();
* and
* void writeByte(unsigned char byte);
*
* (both blocking)
*/

void XBD_loadStringFromConstDataArea( char *dst, const char *src  ) {
  /* copy a zero terminated string from src (CONSTDATAAREA) to dst 
  e.g. strcpy if CONSTDATAREA empty, strcpy_P for PROGMEM
  */
  strcpy(dst,src);
}


void XBD_readPage( uint32_t pageStartAddress, uint8_t * buf )
{
  /* read PAGESIZE bytes from the binary buffer, index pageStartAddress
  to buf */
  memcpy(buf,(uint8_t *)pageStartAddress,PAGESIZE);
}

void XBD_programPage( uint32_t pageStartAddress, uint8_t * buf )
{
  /* copy data from buf (PAGESIZE bytes) to pageStartAddress of
  application binary */
	//static uint32_t last_pageStartAddress=UINT32_MAX;

    //
    // Erase this block of the flash.
    //
    FlashErase(pageStartAddress);
    FlashProgram(buf, pageStartAddress, PAGESIZE);

}

void XBD_switchToApplication()
{
  /* execute the code in the binary buffer */
  // __MSR_MSP(*(unsigned long *)0x2000);
   (*((void (*)(void))(*(unsigned long *)0x2004)))();
}


void XBD_switchToBootLoader()
{
  /* switch back from uploaded code to boot loader */
	  OSRAM96x16x1Init(false);
	  OSRAM96x16x1DisplayOn();
   //
    // Enable the peripherals used by this example.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);

    //
    // Set the period of the watchdog timer.
    //
    WatchdogReloadSet(WATCHDOG0_BASE, 3);

    //
    // Enable reset generation from the watchdog timer.
    //
    WatchdogResetEnable(WATCHDOG0_BASE);

    //
    // Enable the watchdog timer.
    //
    WatchdogEnable(WATCHDOG0_BASE);

	while(1)
	{
		OSRAM96x16x1StringDraw("A", 0, 0);
		OSRAM96x16x1StringDraw("b", 0, 0);
	}
}

uint32_t XBD_busyLoopWithTiming(uint32_t approxCycles)
{
  /* wait for approximatly approxCycles, 
  * then return the number of cycles actually waited */

	volatile uint32_t exactCycles;

 	SysTickDisable();
	SysTickIntDisable();
	SysTickPeriodSet(approxCycles+10000);
	HWREG(NVIC_ST_CURRENT)=0;

	SysTickEnable();
	XBD_sendExecutionStartSignal();

	while( (exactCycles=SysTickValueGet()) > 10000);

	SysTickDisable();
	XBD_sendExecutionCompleteSignal();

	exactCycles = (approxCycles+10000)-SysTickValueGet();
	return exactCycles;
}

void XBD_delayCycles(uint32_t approxCycles)
{
	volatile uint32_t exactCycles;

 	SysTickDisable();
	SysTickIntDisable();
	SysTickPeriodSet(approxCycles+10000);
	HWREG(NVIC_ST_CURRENT)=0;

	SysTickEnable();

	while( (exactCycles=SysTickValueGet()) > 10000);

	SysTickDisable();

	return;
}


uint8_t *p_stack = NULL;
uint8_t *p = NULL;
uint8_t inv_sc=0;


//void getSP(uint8_t **var_SP)
//{
//  volatile uint8_t beacon;
//  *var_SP=&beacon;
//  return;
//}

void XBD_paintStack(void)
{
    register void * __stackptr asm("sp");   ///<Access to the stack pointer
/* initialise stack measurement */
	p = &_end; //bottom of stack, end of bss
	inv_sc=!inv_sc;
	//getSP(&p_stack);
    p_stack = __stackptr;

    
	while(p <= p_stack)
        {
        *p = STACK_CANARY;
        ++p;
        } 
}

uint32_t XBD_countStack(void)
{
/* return stack measurement result */
    p = &_end;
    register uint32_t c = 0;

    while(*p == STACK_CANARY && p <= p_stack)
    {
        ++p;
        ++c;
    }

    return (uint32_t)c;
} 

void XBD_startWatchDog(uint32_t seconds)
{
  (void)seconds;
}
 
void XBD_stopWatchDog()
{
}
