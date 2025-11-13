#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"

/* ============================================================= */
/* PINES UTILIZADOS EN EL PROGRAMA
 *  TIVA PIN  | PUERTO       | FUNCIÓN                     | L293 PIN
 *  ----------|--------------|-----------------------------|---------
 *  PF2       | GPIO_PORTF   | PWM (1-2EN) --> Velocidad     | Pin 1
 *  PC6       | GPIO_PORTC   | 1A --> Dirección (adelante)   | Pin 2
 *  PC7       | GPIO_PORTC   | 2A --> Dirección (atrás)      | Pin 7
 *
 *  CONEXIONES DEL MOTOR:
 *    Motor + --> S1-3 --> L293 Pin 3 (1Y)
 *    Motor – --> S2-3 --> L293 Pin 6 (2Y)
 * ============================================================= */

volatile int Flag_ints = 0;
const int FREC_TIMER = 50;        /* Timer0: 50 Hz = 20 ms */
const int FREC_PWM = 1000;        /* PWM: 1 kHz (suave) */
int RELOJ;
int Load;

// #define SLEEP SysCtlSleep()
#define SLEEP SysCtlSleepFake()

/* SLEEP simulado con Timer0 */
void SysCtlSleepFake(void)
{
    while (!Flag_ints)
        ;
    Flag_ints = 0;
}

/* Interrupción del Timer0 */
void IntTimer0(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Borra flag
    SysCtlDelay(20); // Retraso necesario. Mirar Driverlib p.550
    Flag_ints++;
}

/* PROTOTIPOS */
void motor_Configurar_Pines(void);
void motor_Velocidad(uint8_t porcentaje);
void motor_Encendido_Apagado(uint8_t estado);
void motor_Direccion(uint8_t dir);

/* Configuración del PWM en PF2 (1-2EN del L293) */
void motor_Configurar_Pines(void) {
    // Configurar pines de control de direccion
    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_6 | GPIO_PIN_7);  // 1A y 2A

    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    GPIOPinConfigure(GPIO_PF2_M0PWM2);           // PF2 = M0PWM2
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, RELOJ / FREC_PWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);

    motor_Velocidad(60); // Velocidad inicial
}

int main(void) {
    // Configurar reloj del sistema a 120 MHz
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    /* Habilitar periféricos PRIMERO */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralClockGating(true);

    // Configuración del TIMER0
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ / FREC_TIMER) - 1);
    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer0);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    /* Configurar PWM en PF2 */
    motor_Configurar_Pines();

    /* Iniciar motor */
    motor_Direccion(0);
    motor_Encendido_Apagado(1);
    motor_Velocidad(60);

    while (1) {
        SLEEP;

        static uint32_t contador = 0;
        contador++;
        if (contador >= 100) {
            contador = 0;
            static uint8_t estado = 0;
            estado = (estado + 1) % 4;
            switch (estado) {
            case 0:
                motor_Direccion(0);
                motor_Velocidad(90);
                motor_Encendido_Apagado(1);
                break;
            case 1:
                motor_Direccion(1);
                motor_Velocidad(30);
                motor_Encendido_Apagado(1);
                break;
            case 2:
                motor_Encendido_Apagado(0);
                break;
            case 3:
                motor_Direccion(0);
                motor_Velocidad(30);
                motor_Encendido_Apagado(1);
                break;
            }
        }

        Load = 100 - (100 * TimerValueGet(TIMER0_BASE, TIMER_A)) / ((RELOJ / FREC_TIMER) - 1);
    }
}


void motor_Direccion(uint8_t dir) {
    if (dir == 0) {
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_6, GPIO_PIN_6);  // 1A HIGH
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_7, 0);           // 2A LOW
    } else {
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_6, 0);           // 1A LOW
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_7, GPIO_PIN_7);  // 2A HIGH
    }
}

void motor_Encendido_Apagado(uint8_t estado) {
    if (estado == 1) {
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
    } else {
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, false);
    }
}

void motor_Velocidad(uint8_t porcentaje) {
    if (porcentaje > 100) porcentaje = 100;
    uint32_t ancho = (RELOJ / FREC_PWM) * porcentaje / 100;
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, ancho);
}
