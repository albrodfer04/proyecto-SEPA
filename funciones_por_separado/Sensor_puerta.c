/*
 * main.c
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>


#include "driverlib2.h"
#include "utils/uartstdio.h"

#include "HAL_I2C.h"
#include "sensorlib2.h"

#include "FT800_TIVA.h"


// =======================================================================
// Function Declarations
// =======================================================================


int RELOJ;
volatile int Flag_ints=0;

void SysCtlSleepFake(void)
{
    while(!Flag_ints);
    Flag_ints=0;
}

unsigned long REG_TT[6];
const int32_t REG_CAL[6]= {CAL_DEFAULTS};

//#define SLEEP SysCtlSleep()
#define SLEEP SysCtlSleepFake()

#define NUM_SSI_DATA            3

void IntTimer(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    SysCtlDelay(20); //Retraso necesario. Mirar Driverlib p.550
    Flag_ints++;
}

char puerta_abierta;
void lee_sensores(void);
void dibuja_sensores(void);
int main(void) {


    RELOJ=SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Note: Keep SPI below 11MHz here

    // =======================================================================
    // Delay before we begin to display anything
    // =======================================================================

    SysCtlDelay(RELOJ/3);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, RELOJ/2 -1);
    TimerIntRegister(TIMER0_BASE, TIMER_A,IntTimer);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    UARTStdioConfig(0, 115200, RELOJ);

//    if(Detecta_BP(1))
//    {
//        UARTprintf("\n--------------------------------------");
//        UARTprintf("\n  BOOSTERPACK detectado en posicion 1");
//        UARTprintf("\n   Configurando puerto I2C0");
//        UARTprintf("\n--------------------------------------");
//        Conf_Boosterpack(1, RELOJ);
//    }
//    else if(Detecta_BP(2))
//    {
//        UARTprintf("\n--------------------------------------");
//        UARTprintf("\n  BOOSTERPACK detectado en posicion 2");
//        UARTprintf("\n   Configurando puerto I2C2");
//        UARTprintf("\n--------------------------------------");
//        Conf_Boosterpack(2, RELOJ);
//    }
//    else
//    {
//        UARTprintf("\n--------------------------------------");
//        UARTprintf("\n  Ningun BOOSTERPACK detectado   :-/  ");
//        UARTprintf("\n              Saliendo");
//        UARTprintf("\n--------------------------------------");
//        return 0;
//    }

        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
        GPIOPinTypeGPIOInput(GPIO_PORTA_BASE, GPIO_PIN_7);
        GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

        while(1) {
            SLEEP;
            puerta_abierta = GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_7);
            if(puerta_abierta) {
                // puerta abierta
                UARTprintf("la puerta esta abierta \n");
            } else {
                UARTprintf("la puerta esta cerrada \n");// puerta cerrada
            }
        }

}


//void dibuja_sensores(void)
//{
//    for (i=0;i<4;i++){
//            ComColor(color[i][0],color[i][1],color[i][2]);
//            ComRect(HSIZE/4,30+60*i, HSIZE*3/4,55+60*i, true);
//            ComColor(0,0,0);
//            ComTXT(HSIZE/2,30+60*i+3, 28, OPT_CENTERX,string[i]);
//       }
//}
//void lee_sensores(void)
//{
//    //leo de sensores de luz
//    if(Opt_OK)
//    {
//        lux=OPT3001_getLux();
//        lux_i=(int)round(lux);
//    }
//    //leo temperatura presión y humedad
//    if(Bme_OK)
//    {
//        returnRslt = bme280_read_pressure_temperature_humidity(
//                &g_u32ActualPress, &g_s32ActualTemp, &g_u32ActualHumity);
//        T_act=(float)g_s32ActualTemp/100.0;
//        P_act=(float)g_u32ActualPress/100.0;
//        H_act=(float)g_u32ActualHumity/1000.0;
//    }
//}
