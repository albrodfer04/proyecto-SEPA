// Pines utilizados:
// ------------------------------------------------Sensor de humos
// PD2 --> Entrada analógica AIN13
// PP0 --> Entrada digital DOUT
// ------------------------------------------------Ventilador
// TIVA PIN   | PUERTO       | FUNCIÓN                       | L293 PIN
// PF2       | GPIO_PORTF   | PWM (1-2EN) --> Velocidad     | Pin 1
// PC6       | GPIO_PORTC   | 1A --> Dirección (adelante)   | Pin 2
// PC7       | GPIO_PORTC   | 2A --> Dirección (atrás)      | Pin 7
// CONEXIONES DEL MOTOR:
// Motor + --> S1-3 --> L293 Pin 3 (1Y)
// Motor – --> S2-3 --> L293 Pin 6 (2Y)
// ------------------------------------------------UART
// PA0 --> UART0 RX
// PA1 --> UART0 TX


//-------------------------------------------------------------------LIBRERIAS
#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "utils/uartstdio.h" // UARTprintf()


//------------------------------------------------------------------- Variables globales
int RELOJ;
volatile int Flag_ints = 0;
const int FREC_TIMER = 20;        // 50ms
int Load;

// DETECTOR DE HUMOS
uint32_t adcValue;
volatile int preheating = 0;
const int PREHEAT_TICKS = 600; // 30 segundos (600 × 50 ms)
bool gasDetectadoAnag; //salida analogica
bool gasDetected; //salida digital

// MOTOR
const int FREC_PWM = 1000;        /* PWM: 1 kHz (suave) */

//------------------------------------------------------------------- SLEEP
void SysCtlSleepFake(void)
{
    while (!Flag_ints);
    Flag_ints = 0;
}
//#define SLEEP SysCtlSleep()
#define SLEEP SysCtlSleepFake()

// ISR del Timer0A: se ejecuta cada 50 ms
void IntTimer0(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Limpia flag de interrupción
    SysCtlDelay(20); // Delay necesario por errata en Driverlib (ver p.550)
    Flag_ints++;
}
//------------------------------------------------------------------- PROTOTIPOS
void Reloj_inicializacion(void);
void Inicialización_UART(void);

// Detector de humos
void humo_Detectar(void);
void humo_Inicializar_Pines(void);
void humo_Precalentar(void);

// Motor
void motor_Configurar_Pines(void);
void motor_Velocidad(uint8_t porcentaje);
void motor_Encendido_Apagado(uint8_t estado);
void motor_Direccion(uint8_t dir);

int main(void)
{
    Reloj_inicializacion();
    Inicialización_UART();

    humo_Inicializar_Pines();

    // Activar periféricos durante modo Sleep
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralClockGating(true);

    // Inicializaion de sensores y actuadores
    motor_Configurar_Pines();
    motor_Direccion(0);
    motor_Encendido_Apagado(0);
    motor_Velocidad(0);
    humo_Precalentar();

    while (1)
    {
        SLEEP; // Simulación de modo bajo consumo (50 ms)
        humo_Detectar();
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
//--------------------------------------------------------------- MOTOR
void motor_Configurar_Pines(void) {

    // Configurar pines de control de direccion
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_6 | GPIO_PIN_7);  // 1A y 2A

    // PWM (PF2)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    GPIOPinConfigure(GPIO_PF2_M0PWM2);           // PF2 = M0PWM2
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, RELOJ / FREC_PWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
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

//--------------------------------------------------------------- HUMO
void humo_Precalentar(void){
    while (preheating < PREHEAT_TICKS) {
        SLEEP;  // 50 ms en bajo consumo
        preheating++;
        if ((preheating % 20) == 0) {  // Cada segundo
            UARTprintf("\033[2K\rPrecalentando sensor de humos... %d s", preheating / 20);
        }
    }
    UARTprintf("\033[2K\rSensor listo. Iniciando deteccion...     \n");
}

void humo_Detectar(void){
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
void humo_Inicializar_Pines(void){
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




//--------------------------------------------------------------- RELOJ Y UART
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
void Reloj_inicializacion(void){  // 50 ms
    // Configuración del reloj del sistema a 120 MHz usando PLL
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Activación de Timer
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); // Timer0A

    // Configuración del Timer0A para generar interrupciones cada 50 ms
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ / FREC_TIMER) - 1); // 50 ms
    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer0);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);
}

