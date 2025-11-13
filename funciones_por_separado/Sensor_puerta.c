// =======================================================================
// Pines utilizados:
// UART0: PA0 (RX), PA1 (TX)
// Entrada digital (puerta): PA7
// =======================================================================

/*
 * main.c
 */

#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "utils/uartstdio.h"


int RELOJ; // frecuencia del sistema
volatile int Flag_ints=0; // flag para control de interrupciones

void SysCtlSleepFake(void)
{
    while(!Flag_ints);
    Flag_ints=0;
}

//#define SLEEP SysCtlSleep()
#define SLEEP SysCtlSleepFake()

void IntTimer(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // limpia flag
    SysCtlDelay(20); // delay necesario (ver driverlib)
    Flag_ints++; // activa flag
}

char puerta_abierta; // variable para estado de puerta


int main(void) {

    // Configura reloj a 120 MHz con PLL
    RELOJ=SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Espera inicial antes de mostrar nada
    SysCtlDelay(RELOJ/3);

    // Configuración del temporizador
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, RELOJ/2 -1);
    TimerIntRegister(TIMER0_BASE, TIMER_A,IntTimer);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    // Configuración UART0 en PA0 y PA1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, RELOJ); // UART a 115200 baudios

    // Configura PA7 como entrada con pull-up
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeGPIOInput(GPIO_PORTA_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Bucle principal
    while(1) {
        SLEEP; // espera a interrupción
        puerta_abierta = GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_7); // lee PA7
        if(puerta_abierta) {
            UARTprintf("la puerta esta abierta \n");
        } else {
            UARTprintf("la puerta esta cerrada \n");
        }
    }
}


