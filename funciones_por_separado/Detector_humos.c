#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "utils/uartstdio.h" // UARTprintf()
// Variables globales
int RELOJ;
volatile int Flag_ints = 0;
uint32_t adcValue;
volatile int preheating = 0;
const int PREHEAT_TICKS = 600; // 30 segundos (600 × 50 ms)
bool gasDetectadoAnag;
bool gasDetected; //salida digital
// Simulación de modo bajo consumo
#define SLEEP SysCtlSleepFake()
void SysCtlSleepFake(void)
{
    while (!Flag_ints);
    Flag_ints = 0;
}
// ISR del Timer0A: se ejecuta cada 50 ms
void IntTimer0(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Limpia flag de interrupción
    SysCtlDelay(20); // Delay necesario por errata en Driverlib (ver p.550)
    Flag_ints++;
}
//--------PROTOTIPOS
void Detectar_humo(void);
void Inicializacion_pines_sensor_humos(void);
void Inicialización_UART(void);
void Precalentar_sensor_humos(void);

int main(void)
{
    // Configuración del reloj del sistema a 120 MHz usando PLL
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Activación de Timer
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); // Timer0A

    // Configuración del Timer0A para generar interrupciones cada 50 ms
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ / 20) - 1); // 50 ms
    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer0);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    Inicialización_UART();
    Inicializacion_pines_sensor_humos();

    // Activar periféricos durante modo Sleep
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralClockGating(true);

    Precalentar_sensor_humos();

    while (1)
    {
        SLEEP; // Simulación de modo bajo consumo (50 ms)
        Detectar_humo();
    }
}

void Precalentar_sensor_humos(void){
    while (preheating < PREHEAT_TICKS) {
        SLEEP;  // 50 ms en bajo consumo
        preheating++;
        if ((preheating % 20) == 0) {  // Cada segundo
            UARTprintf("\033[2K\rPrecalentando sensor de humos... %d s", preheating / 20);
        }
    }
    UARTprintf("\033[2K\rSensor listo. Iniciando deteccion...     \n");
}

void Detectar_humo(void){
    // Lectura del canal analógico AIN13 (PD2)
    ADCProcessorTrigger(ADC0_BASE, 3);
    while (!ADCIntStatus(ADC0_BASE, 3, false));
    ADCIntClear(ADC0_BASE, 3);
    ADCSequenceDataGet(ADC0_BASE, 3, &adcValue);
    // Lectura del pin digital PP0 (DOUT del sensor)
    gasDetected = (GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_0) == 0);
    // Activación de LEDs según estado del sensor
    if (adcValue > 870) {
        gasDetectadoAnag=1;
    } else {
        gasDetectadoAnag=0;
    }
    // Salida por UART
    UARTprintf("\033[2K\rADC: %u\tDOUT: %s DIGITAL), ESTADO: %s", adcValue, gasDetected ? "HIGH (Gas" : "LOW (Clean",gasDetectadoAnag ? "ALARMA GAS DETECTADO" : "AIRE LIMPIO");
}
void Inicializacion_pines_sensor_humos(void){
    // Activación de periféricos necesarios
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); // PD2 = AIN13 (ADC)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP); // PP0 = DOUT (digital)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // ADC0
    // Configuración del ADC0 para canal AIN13 (PD2)
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH13 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    // Configuración de pines
    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_2); // PD2 como entrada analógica
    GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_0); // PP0 como entrada digital
    GPIOPadConfigSet(GPIO_PORTP_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
}
void Inicialización_UART(void){
    // Activación de periféricos necesarios
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); // UART0 (PA0/PA1)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); // UART0
    // Configuración UART0: PA0 = RX, PA1 = TX
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, RELOJ); // UART0 a 115200 baudios
}
