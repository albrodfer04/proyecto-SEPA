// =======================================================================
// Pines utilizados:
// PG1 --> PWM Rojo (PWM_OUT_5)
// PK4 --> PWM Verde (PWM_OUT_6)
// PK5 --> PWM Azul (PWM_OUT_7)
// =======================================================================

#include <stdint.h>
#include <stdbool.h>
//#include <stdio.h>

#include "driverlib2.h"
#include "utils/uartstdio.h"

int RELOJ;
int Dutymax = 255;
int Dutymin = 0;

void led(int DutyRojo, int DutyVerde, int DutyAzul);
void led_Inicializar_Pines(void);

int main(void) {
    // Configura reloj a 120 MHz con PLL
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Inicializa PWM y pines
    led_Inicializar_Pines();

    int DutyRojo = 1, DutyVerde = 1, DutyAzul = 1, cuenta = 0;

    // Bucle principal: cambia color cada ~RELOJ/20 ciclos
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
        } else if (cuenta < 80) {
            DutyRojo = 150;
            DutyVerde = 100;
            DutyAzul = 100;
        } else {
            cuenta = 0;
        }
        led(DutyRojo, DutyVerde, DutyAzul); // actualiza PWM
        SysCtlDelay(RELOJ / 20); // delay para notar el cambio
    }
}

void led_Inicializar_Pines(void) {
    int PeriodoPWM = 255;

    // Activa periféricos necesarios
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_4); // divide reloj PWM

    // Configura pines PG1, PK4, PK5 como salidas PWM
    GPIOPinConfigure(GPIO_PG1_M0PWM5); // Rojo
    GPIOPinConfigure(GPIO_PK4_M0PWM6); // Verde
    GPIOPinConfigure(GPIO_PK5_M0PWM7); // Azul
    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // PWM Gen 2 --> Rojo (PWM_OUT_5)
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);

    // PWM Gen 3 --> Verde y Azul (PWM_OUT_6 y PWM_OUT_7)
    PWMGenConfigure(PWM0_BASE, PWM_GEN_3, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_3);
}

void led(int DutyRojo, int DutyVerde, int DutyAzul) {
    // Rojo (PWM_OUT_5)
    if (DutyRojo <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, false);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
        if (DutyRojo >= Dutymax) DutyRojo = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, DutyRojo);
    }

    // Verde (PWM_OUT_6)
    if (DutyVerde <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, false);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, true);
        if (DutyVerde >= Dutymax) DutyVerde = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_6, DutyVerde);
    }

    // Azul (PWM_OUT_7)
    if (DutyAzul <= Dutymin) {
        PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, true);
        if (DutyAzul >= Dutymax) DutyAzul = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, DutyAzul);
    }
}
