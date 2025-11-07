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
int RELOJ;
int Dutymax=255;
int Dutymin=0;
void led(int DutyRojo, int DutyVerde, int DutyAzul);
int main(void) {
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);
    int PeriodoPWM = 255;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_4);

    GPIOPinConfigure(GPIO_PF1_M0PWM1);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinConfigure(GPIO_PF3_M0PWM3);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    // Generador 0 para Rojo
    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_0);

    // Generador 1 para Verde y Azul
    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);

    int DutyRojo = 1, DutyVerde = 1, DutyAzul = 1, cuenta = 0;

    while (1) {
        cuenta++;
        if (cuenta < 20) {
            DutyRojo = 255;
            DutyVerde = 0;
            DutyAzul = 0;
        } else if (cuenta < 40) {
            DutyRojo = 0;
            DutyVerde = 180;
            DutyAzul = 0;
        } else if (cuenta < 60) {
            DutyRojo = 0;
            DutyVerde = 0;
            DutyAzul = 220;

        }else if (cuenta< 80){
            DutyRojo = 150;
            DutyVerde = 100;
            DutyAzul = 100;
        }else{
            cuenta = 0;
        }
        led(DutyRojo, DutyVerde, DutyAzul);
        SysCtlDelay(RELOJ / 20); // Delay para que se note el cambio
    }
}
void led(int DutyRojo, int DutyVerde, int DutyAzul) {
    if (DutyRojo <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, false);  // apaga salida rojo
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, true);
        if(DutyRojo>=Dutymax) DutyRojo=Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, DutyRojo);
    }
    // Lo mismo para verde y azul
    if (DutyVerde <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, false);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
        if(DutyVerde>=Dutymax) DutyVerde=Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, DutyVerde);
    }
    if (DutyAzul <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_3_BIT, false);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_3_BIT, true);
        if(DutyAzul>=Dutymax) DutyAzul=Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_3, DutyAzul);
    }
}

