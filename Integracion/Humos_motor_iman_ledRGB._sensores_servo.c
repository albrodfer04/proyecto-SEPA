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
// -----------------------------------------------IMAN
// PA7 --> Entrada digital
// -----------------------------------------------LED RGB
// PG1 --> PWM Rojo (PWM_OUT_5)
// PK4 --> PWM Verde (PWM_OUT_6)
// PK5 --> PWM Azul (PWM_OUT_7)
// -----------------------------------------------UART
// PA0 --> UART0 RX
// PA1 --> UART0 TX
//------------------------------------------------SERVO PERSIANAS
// PF1 --> Posición del servo  GPIO_PORTF, PWM_OUT_1 generador 0

//-------------------------------------------------------------------LIBRERIAS
#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "utils/uartstdio.h" // UART

#include <stdio.h>
#include <math.h> //para calculos

#include "HAL_I2C.h"
#include "sensorlib2.h" //sensores boosterpack

#include "FT800_TIVA.h" //pantalla

//------------------------------------------------------------------- Variables globales
int RELOJ;
volatile int Flag_ints = 0;
const int FREC_TIMER = 20;        // 50ms
int Load;

// DETECTOR DE HUMOS------------------------------------------------
uint32_t adcValue;
volatile int preheating = 0;
const int PREHEAT_TICKS = 600; // 30 segundos (600 × 50 ms)
bool gasDetectadoAnag; //salida analogica
bool gasDetected; //salida digital

// MOTOR------------------------------------------------------------
const int FREC_PWM = 1000;        /* PWM: 1 kHz (suave) */

// IMAN-------------------------------------------------------------
char puerta_abierta; // variable para estado de puerta

// LED--------------------------------------------------------------
int Dutymax = 255;
int Dutymin = 0;
// sensores temperatura y luz:--------------------------------------

int DevID=0;
//OPT3000
float lux; //valor de lectura luz
int lux_i;

// BME280
int returnRslt;
int g_s32ActualTemp   = 0;
unsigned int g_u32ActualPress  = 0;
unsigned int g_u32ActualHumity = 0;
// struct bme280_t bme280;
float T_act;
bool BME_on = true;

//valores inicializacion
uint8_t Sensor_OK=0;
#define BP 2
uint8_t Opt_OK, Bme_OK;

//SERVO------------------------------------------------------------
volatile int Max_pos = 4200; //3750
volatile int Min_pos = 1300; //1875
int PeriodoPWM;
volatile int pos_180=0;
volatile int posicion;
int ang;

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

// Iman
void puerta_Configurar_Pines(void);
void puerta_Detectar(void);

// LED
void led(int DutyRojo, int DutyVerde, int DutyAzul);
void led_Inicializar_Pines(void);

//sensores temperatura y luz
void sensores_booster_inic(void);
void lee_sensores(void);

//servos persianas
void Servo_inic(void);
void Servo(int angulo_objetivo);

int main(void)
{
    Reloj_inicializacion();
    Inicialización_UART();

    humo_Inicializar_Pines();
    motor_Configurar_Pines();
    puerta_Configurar_Pines();
    led_Inicializar_Pines();
    sensores_booster_inic();
    Servo_inic();

    // Activar periféricos durante modo Sleep
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOK);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralClockGating(true);

    // Inicializaion de sensores y actuadores
    motor_Direccion(0);
    motor_Encendido_Apagado(0);
    motor_Velocidad(0);
    led(50, 50, 50);
    humo_Precalentar();
    Servo(0);
    while (1)
    {
        SLEEP; // Simulación de modo bajo consumo (50 ms)
        humo_Detectar();
        puerta_Detectar();
        lee_sensores();
        Servo(ang);
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
                ang=0;
                led(255, 0, 0);
                break;
            case 1:
                motor_Direccion(1);
                motor_Velocidad(30);
                motor_Encendido_Apagado(1);
                ang=45;
                led(0, 255, 0);

                break;
            case 2:
                motor_Encendido_Apagado(0);
                ang=90;
                led(0, 0, 255);
                break;
            case 3:
                motor_Direccion(0);
                motor_Velocidad(30);
                motor_Encendido_Apagado(1);
                ang=170;
                led(0, 0, 0);
                break;
            }
        }
        Load = 100 - (100 * TimerValueGet(TIMER0_BASE, TIMER_A)) / ((RELOJ / FREC_TIMER) - 1);
    }
}
//--------------------------------------------------------------- LED
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
//--------------------------------------------------------------- IMAN
void puerta_Configurar_Pines(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeGPIOInput(GPIO_PORTA_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}
void puerta_Detectar(void){
    puerta_abierta = GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_7); // lee PA7
    if(puerta_abierta) {
        UARTprintf("la puerta esta abierta \n");
    } else {
        UARTprintf("la puerta esta cerrada \n");
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
    SysCtlDelay(RELOJ/3);

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



//sensores temperatura y luz--------------------------------------------------------------------------------------------------

void sensores_booster_inic(void)
{

    if(Detecta_BP(1))
    {
        UARTprintf("\n--------------------------------------");
        UARTprintf("\n  BOOSTERPACK detectado en posicion 1");
        UARTprintf("\n   Configurando puerto I2C0");
        UARTprintf("\n--------------------------------------");
        Conf_Boosterpack(1, RELOJ);
    }
    else if(Detecta_BP(2))
    {
        UARTprintf("\n--------------------------------------");
        UARTprintf("\n  BOOSTERPACK detectado en posicion 2");
        UARTprintf("\n   Configurando puerto I2C2");
        UARTprintf("\n--------------------------------------");
        Conf_Boosterpack(2, RELOJ);
    }
    else
    {
        UARTprintf("\n--------------------------------------");
        UARTprintf("\n  Ningun BOOSTERPACK detectado   :-/  ");
        UARTprintf("\n              Saliendo");
        UARTprintf("\n--------------------------------------");
    }

    UARTprintf("\033[2J \033[1;1H Inicializando OPT3001... ");
    Sensor_OK=Test_I2C_Dir(OPT3001_SLAVE_ADDRESS); //vemos que hay dispositivo en esa dirección
    if(!Sensor_OK)
    {
        UARTprintf("Error en OPT3001\n");
        Opt_OK=0;

    }
    else
    {
        OPT3001_init(); //inicializo dispositivo
        UARTprintf("Hecho!\n");
        UARTprintf("Leyendo DevID... ");
        DevID=OPT3001_readDeviceId();
        UARTprintf("DevID= 0X%x \n", DevID);
        Opt_OK=1;
    }


    UARTprintf("Inicializando BME280... ");
    Sensor_OK=Test_I2C_Dir(BME280_I2C_ADDRESS2);
    if(!Sensor_OK)
    {
        UARTprintf("Error en BME280\n");
        Bme_OK=0;
    }
    else
    {
        bme280_data_readout_template();
        bme280_set_power_mode(BME280_NORMAL_MODE);
        UARTprintf("Hecho! \nLeyendo DevID... ");
        readI2C(BME280_I2C_ADDRESS2,BME280_CHIP_ID_REG, &DevID, 1);
        UARTprintf("DevID= 0X%x \n", DevID);
        Bme_OK=1;
    }

}

void lee_sensores(void)
{
    char buffer[32];
    //leo de sensores de luz
    if(Opt_OK)
    {
        lux=OPT3001_getLux();
        lux_i=(int)round(lux);
        UARTprintf(" OPT300: %d ",lux_i);
    }
    //leo temperatura
    if(Bme_OK)
    {
        returnRslt = bme280_read_pressure_temperature_humidity(
                &g_u32ActualPress, &g_s32ActualTemp, &g_u32ActualHumity);
        T_act=(float)g_s32ActualTemp/100.0;

        sprintf(buffer, " Temperatura: %.2f C ", T_act);
        UARTprintf("%s", buffer);
    }
}



// SERVO-------------------------------------------------------------------
//inicializacion
void Servo_inic(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE,PWM_SYSCLK_DIV_64);   // al PWM le llega un reloj de 1.875MHz
    //configuración pin PF1
    GPIOPinConfigure(GPIO_PF1_M0PWM1);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1);

    //Configurar el pwm0, contador descendente y sin sincronización (actualización automática)
    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    //valores iniciales------------------------
    PeriodoPWM=37499; // 50Hz  a 1.875MHz
    pos_180=0; //Inicialmente
    posicion=Min_pos+((Max_pos-Min_pos)*pos_180)/180;
    //-------------------------------------------
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, PeriodoPWM); //frec:50Hz
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, posicion);   //Inicialmente, 1ms
    PWMGenEnable(PWM0_BASE, PWM_GEN_0);     //Habilita el generador 0
    PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT , true);    //Habilita la salida 1

}

void Servo(int angulo_objetivo) {
    int paso = 5; // tamaño del paso (grados por actualización)
//comprobamos que este en rango:
    if(angulo_objetivo>180) angulo_objetivo=180;
    if(angulo_objetivo<0) angulo_objetivo=0;
//ajuste suave
        if (pos_180 < angulo_objetivo) {
            pos_180 += paso;
            if (pos_180 > angulo_objetivo) pos_180 = angulo_objetivo; //ya ha llegado al ángulo
        }
        else if (pos_180 > angulo_objetivo) {
            pos_180 -= paso;
            if (pos_180 < angulo_objetivo) pos_180 = angulo_objetivo;
        }
        else
            pos_180 = angulo_objetivo;
    //actualización de servo
    posicion=Min_pos+((Max_pos-Min_pos)*pos_180)/180;
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, posicion);

}

