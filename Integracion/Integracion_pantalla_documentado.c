/**
 * @file Integracion_pantalla_documentado.c
 * @brief Sistema integrado: sensores, pantalla FT800, servo, motor, LED RGB y UART.
 *
 * Implementa:
 *  - Lectura de sensores (MQ-x, BME280, OPT3001)
 *  - Control de actuadores (motor DC, servo persianas, LED RGB)
 *  - Gestion del Boosterpack (I2C)
 *  - Comunicacion UART
 *  - Interfaz grafica FT800
 *  - Bucle principal sincronizado mediante Timer (50 ms)
 *
 * @authors
 *  - Alba R.
 *  - Alejandra D-C.
 *
 * @date Noviembre 2025
 * @section mapa_de_pines Mapa de pines utilizados
 *
 * Sensor de humos:
 *  - PD2 -> AIN13 (ADC)
 *  - PP0 -> DOUT (digital)
 *
 * Motor DC (L293):
 *  - PF2 -> PWM velocidad (1-2EN)
 *  - PC6 -> 1A (adelante)
 *  - PC7 -> 2A (atras)
 *
 * Iman (puerta):
 *  - PA7 -> entrada digital
 *
 * LED RGB:
 *  - PG1 -> PWM rojo (PWM_OUT_5)
 *  - PK4 -> PWM verde (PWM_OUT_6)
 *  - PK5 -> PWM azul (PWM_OUT_7)
 *
 * UART0:
 *  - PA0 -> RX
 *  - PA1 -> TX
 *
 * Servo persianas:
 *  - PF1 -> PWM_OUT_1
 */

//------------------------------------------------------------------- LIBRERIAS
#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "utils/uartstdio.h" // UART
#include <stdio.h>
#include <math.h>
#include "HAL_I2C.h"
#include "sensorlib2.h" // sensores boosterpack
#include "FT800_TIVA.h" // pantalla
#include <string.h>
#include <stdlib.h>

//------------------------------------------------------------------- VARIABLES GLOBALES
int RELOJ;
volatile int Flag_ints = 0;
const int FREC_TIMER = 20;        // 50 ms
int Load;
int hora;

/* DETECTOR DE HUMOS */
uint32_t adcValue;
volatile int preheating = 0;
const int PREHEAT_TICKS = 600; // 30 segundos (600 * 50 ms)
bool gasDetectadoAnag; // salida analogica
bool gasDetected;      // salida digital

/* MOTOR ventilador */
const int FREC_PWM = 1000; /* PWM: 1 kHz */
int estado_ventilador=0;
int cuenta_vent;
#define U_Temp_MAX 30
#define U_Temp_MIN 24
/* IMAN */
char puerta_abierta; // estado de puerta

/* LED */
int Dutymax = 255;
int Dutymin = 0;
int estado_luz=0;
int estado_luz_ant=0;
int cuenta_led=0;
#define U_LUZ_MIN 50
#define U_LUZ_MAX 500

/* SENSORES TEMPERATURA Y LUZ */
int DevID = 0;
/* OPT3001 */
float lux;
int lux_i;

/* BME280 */
int returnRslt;
int g_s32ActualTemp   = 0;
unsigned int g_u32ActualPress  = 0;
unsigned int g_u32ActualHumity = 0;
float T_act;
bool BME_on = true;

/* inicializacion sensores */
uint8_t Sensor_OK = 0;
#define BP 2
uint8_t Opt_OK, Bme_OK;

/* SERVO */
volatile int Max_pos = 4200; // 3750
volatile int Min_pos = 1300; // 1875
int PeriodoPWM;
volatile int pos_180 = 0;
volatile int posicion;
int ang;
int estado_persianas=0;
int cuenta_per=0;

/* PANTALLA */
int cont; // contador de mensajes
char buffer[2][20]; // mensajes a escribir

#define dword long
#define byte char
#define MSEC 40000 // valor para 1ms con SysCtlDelay() 120MHz/40000*3 del delay = 1ms

unsigned long REG_TT[6];
const int32_t REG_CAL[6] = { CAL_DEFAULTS };
#define NUM_SSI_DATA 3

/* COMUNICACION / ESTADOS */
int lampara = 0;
int ventilador = 0;
int pastilla = 0;
int comida = 0;
int ayuda = 0;
int emergencia = 0;

int estado_pastilla=0;
int estado_comida=0;

//------------------------------------------------------------------- SLEEP / ISR

/**
 * @brief Simula modo Sleep: bloquea hasta el flag de interrupcion.
 *
 * La rutina espera a que el timer incremente Flag_ints y lo limpia.
 */
void SysCtlSleepFake(void)
{
    while (!Flag_ints);
    Flag_ints = 0;
}

#define SLEEP SysCtlSleepFake()

/**
 * @brief ISR del Timer0A (periodo 50 ms).
 *
 * Limpia la interrupcion, aplica pequeno delay por errata y
 * aumenta el contador de sincronizacion.
 */
void IntTimer0(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    SysCtlDelay(20); // errata driverlib
    Flag_ints++;
}

//------------------------------------------------------------------- PROTOTIPOS

void Reloj_inicializacion(void);
void Inicializacion_UART(void);
//void Estado_uart(void);

/* Detector de humos */
void humo_Detectar(void);
void humo_Inicializar_Pines(void);
void humo_Precalentar(void);

/* Motor */
void motor_Configurar_Pines(void);
void motor_Velocidad(uint8_t porcentaje);
void motor_Encendido_Apagado(uint8_t estado);
void motor_Direccion(uint8_t dir);

/* Iman */
void puerta_Configurar_Pines(void);
void puerta_Detectar(void);

/* LED */
void led(int DutyRojo, int DutyVerde, int DutyAzul);
void led_Inicializar_Pines(void);

/* Sensores boosterpack */
void sensores_booster_inic(void);
void lee_sensores(void);

/* Servo persianas */
void Servo_inic(void);
void Servo(int angulo_objetivo);

/* Pantalla FT800 */

void inicia_pantalla(void);
void dibuja_pantalla(void);
void MandaMensaje(char *s);
void DibujaMensaje(void);
void DibujaRecordatorios(int pastilla, int comida);
void DibujaBotones(void);
void DatosPantalla(void);

/**
 * @brief Funcion principal.
 *
 * Inicializa modulos y ejecuta bucle periodico.
 * @return no retorna
 */
int main(void)
{
    int i;
    Reloj_inicializacion();
    inicia_pantalla();
    Inicializacion_UART();

    humo_Inicializar_Pines();
    motor_Configurar_Pines();
    puerta_Configurar_Pines();
    led_Inicializar_Pines();
    sensores_booster_inic();
    Servo_inic();

    /* Activar perifericos durante Sleep */
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

    /* Inicializacion de actuadores y pantalla */
    motor_Direccion(0);
    motor_Encendido_Apagado(0);
    motor_Velocidad(0);
    led(50, 50, 50);
    MandaMensaje("Puesta en marcha");
    dibuja_pantalla();
    humo_Precalentar();
    Servo(0);

    Espera_pant();
    for (i = 0; i < 6; i++)
        Esc_Reg(REG_TOUCH_TRANSFORM_A + 4 * i, REG_CAL[i]);

    while (1)
    {
        SLEEP; // 50 ms
        humo_Detectar();
        puerta_Detectar();
        lee_sensores();
        Servo(ang);
        DatosPantalla();

        /** Maquina de estado Luz----------------------------------------------------------------------------------------------------------------*/
        switch(estado_luz){
        case 0:
            led(0, 0, 0);
            if(lampara==1) {
                MandaMensaje("Luz encendida");
                estado_luz=4;
                lampara=1;
            }
            if (lux<U_LUZ_MIN) {
                estado_luz=1;
                lampara=1;
            }
            if (comida==1){
                estado_luz_ant=estado_luz;
                estado_luz=2;
                lampara=1;
            }
            if (pastilla==1){
                estado_luz_ant=estado_luz;
                estado_luz=3;
                lampara=1;
            }
            break;

        case 1:
            led(100,100,100); //luz al 50%
            if(lampara==0) estado_luz=0;
            if (comida==1){
                estado_luz_ant=estado_luz;
                estado_luz=2;
                lampara=1;
            }
            if(pastilla==1){
                estado_luz_ant=estado_luz;
                estado_luz=3;
                lampara=1;
            }
            break;
        case 2:
            led(255,0,255);
            cuenta_led++;
            if(comida==0)
            {
                if(!estado_luz_ant) lampara=0;
                cuenta_led=0;
                estado_luz=estado_luz_ant;
            }
            break;
        case 3:
            led(255,255,0);
            cuenta_led++;
            if(pastilla==0)
            {
                if(!estado_luz_ant) lampara=0;
                cuenta_led=0;
                estado_luz=estado_luz_ant;
            }
            break;
        case 4:
            led(255,255,255);
            if(!lampara){estado_luz=0;}
            if(lux>U_LUZ_MAX){
                lampara=0;
                estado_luz=0;}
            if (comida==1){
                estado_luz_ant=estado_luz;
                estado_luz=2;
                lampara=1;
            }
            if(pastilla==1){
                estado_luz_ant=estado_luz;
                estado_luz=3;
                lampara=1;
            }
            break;
        }

        /** Maquina estado pastilla-----------------------------------------------------------------------*/
        switch(estado_pastilla)
        {
        case 0://sin pastilla activado
            if(pastilla==1){
                MandaMensaje("Toma pastilla");
                estado_pastilla=1;
            }
            break;
        case 1:
            if(pastilla==0){
                //manda pastilla tomada
                estado_pastilla=0;
            }
            break;
        }

        /** Maquina estado comida-----------------------------------------------------------------------*/
        switch(estado_comida)
        {
        case 0://sin pastilla activado
            if(comida==1){
                MandaMensaje("Toma comida");
                estado_comida=1;
            }
            break;
        case 1:
            if(comida==0){
                //manda comida tomada a web
                estado_comida=0;
            }
            break;
        }


        /**Maquina estados ventilador--------------------------------------------------------------------*/
        switch(estado_ventilador)
        {
        case 0: //ventilador apagado
            motor_Direccion(0);
            motor_Velocidad(90);
            motor_Encendido_Apagado(0);
            if(ventilador) estado_ventilador=1;
            if(T_act>U_Temp_MAX){ //primeros encendemos a bajo
                ventilador=1;
                estado_ventilador=1;
            }
            //poner si se enciende de web
            break;
        case 1:
            motor_Direccion(0);
            motor_Velocidad(20);
            motor_Encendido_Apagado(1);
            if(!ventilador) estado_ventilador=0;
            if(T_act>U_Temp_MAX){
                cuenta_vent++;
                if(cuenta_vent>50){
                    estado_ventilador++;
                    cuenta_vent=0;
                }
            }
            if(T_act<U_Temp_MIN){
                estado_ventilador--;
                ventilador=0;
            }
            break;
        case 2:
            motor_Direccion(0);
            motor_Velocidad(50);
            motor_Encendido_Apagado(1);
            if(!ventilador) estado_ventilador=0;
            if(T_act>U_Temp_MAX){
                cuenta_vent++;
                if(cuenta_vent>50){
                    estado_ventilador++;
                    cuenta_vent=0;
                }

            }
            if(T_act<U_Temp_MIN) estado_ventilador--;
            break;
        case 3:
            motor_Direccion(0);
            motor_Velocidad(80);
            motor_Encendido_Apagado(1);
            if(!ventilador) estado_ventilador=0;
            if(T_act<U_Temp_MIN) estado_ventilador--;
            break;
        }

        /**maquina estado persianas-----------------------------------------------------------------------*/

        switch(estado_persianas){
        case 0: //bajadas
            ang = 0;
            if((hora==9)){//añadir que si se baja desde la web que no se meta en esto
                estado_persianas=1;
                MandaMensaje("Ya es de dia");
            }
            if((lux<U_LUZ_MIN)&&(hora<20)) //poca luz y es de dia
            {
                estado_persianas=1;
            }
            break;
        case 1:
            ang = 90;
            if((lux<U_LUZ_MIN)&&(hora<20)) //poca luz y es de dia
            {
                cuenta_per++;
                if(cuenta_per>=50){
                    cuenta_per=0;
                    estado_persianas=2;
                }
            }
            if((lux>U_LUZ_MAX)) //mucha luz y es de dia
            {
                cuenta_per++;
                if(cuenta_per>=50){
                    cuenta_per=0;
                    estado_persianas=0;
                }
            }
            //añadir que se bajen desde web
            break;
        case 2:
            ang = 180;
            if((lux>U_LUZ_MAX)) //mucha luz y es de dia
            {
                cuenta_per++;
                if(cuenta_per>=50){
                    cuenta_per=0;
                    estado_persianas=1;
                }
            }
            if(hora>20){
                estado_persianas=0;
                MandaMensaje("Ya es de noche");
            }
            break;
        }
        /** Gestion personas-------------------------------------------------------------*/

        static uint32_t contador = 0;
        contador++;
        if (contador >= 100)
        {
            contador = 0;
            static uint8_t estado = 0;
            estado = (estado + 1) % 4;
            switch (estado)
            {
            case 0:
                hora=8;

                pastilla = 1;
                break;
            case 1:
                hora=9;
                ang = 45;

                break;
            case 2:
                hora=16;
                ang = 90;
                comida=1;
                break;
            case 3:
                hora=21;
                ang = 170;

                break;
            default:
                break;
            }
        }
        // Estado_uart();
        dibuja_pantalla();
        Load = 100 - (100 * TimerValueGet(TIMER0_BASE, TIMER_A)) / ((RELOJ / FREC_TIMER) - 1);
    }
}


/**
 * @brief Inicializa los pines PWM del LED RGB (PG1, PK4, PK5).
 *
 * Configura generadores PWM y periodos.
 */
void led_Inicializar_Pines(void)
{
    int PeriodoPWM = 255;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_4); // divide reloj PWM

    GPIOPinConfigure(GPIO_PG1_M0PWM5); // Rojo
    GPIOPinConfigure(GPIO_PK4_M0PWM6); // Verde
    GPIOPinConfigure(GPIO_PK5_M0PWM7); // Azul
    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_3, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, PeriodoPWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_3);
}

/**
 * @brief Ajusta el brillo de cada canal del LED RGB.
 *
 * @param DutyRojo 0-255 intensidad rojo
 * @param DutyVerde 0-255 intensidad verde
 * @param DutyAzul 0-255 intensidad azul
 */
void led(int DutyRojo, int DutyVerde, int DutyAzul)
{
    if (DutyRojo <= Dutymin)
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, false);
    }
    else
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
        if (DutyRojo >= Dutymax)
            DutyRojo = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, DutyRojo);
    }

    if (DutyVerde <= Dutymin)
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, false);
    }
    else
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, true);
        if (DutyVerde >= Dutymax)
            DutyVerde = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_6, DutyVerde);
    }

    if (DutyAzul <= Dutymin)
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
    }
    else
    {
        PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, true);
        if (DutyAzul >= Dutymax)
            DutyAzul = Dutymax;
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, DutyAzul);
    }
}


/**
 * @brief Configura el pin PA7 como entrada digital con pull-up.
 */
void puerta_Configurar_Pines(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeGPIOInput(GPIO_PORTA_BASE, GPIO_PIN_7);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

/**
 * @brief Lee el estado del sensor iman (puerta) y lo imprime por UART.
 *
 * Actualiza la variable global `puerta_abierta`.
 */
void puerta_Detectar(void)
{
    puerta_abierta = GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_7); // lee PA7
    if (puerta_abierta)
    {
        UARTprintf("la puerta esta abierta \n");
    }
    else
    {
        UARTprintf("la puerta esta cerrada \n");
    }
}


/**
 * @brief Configura pines de direccion y PWM del motor DC.
 *
 * PF2 -> PWM, PC6/PC7 -> direccion.
 */
void motor_Configurar_Pines(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_6 | GPIO_PIN_7);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, RELOJ / FREC_PWM);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
}

/**
 * @brief Establece la direccion del motor.
 * @param dir 0 = adelante, 1 = atras
 */
void motor_Direccion(uint8_t dir)
{
    if (dir == 0)
    {
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_6, GPIO_PIN_6);
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_7, 0);
    }
    else
    {
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_6, 0);
        GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_7, GPIO_PIN_7);
    }
}

/**
 * @brief Enciende o apaga el motor mediante PWM.
 * @param estado 1 = encendido, 0 = apagado
 */
void motor_Encendido_Apagado(uint8_t estado)
{
    if (estado == 1)
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
    else
        PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, false);
}

/**
 * @brief Ajusta la velocidad del motor DC.
 * @param porcentaje 0-100
 */
void motor_Velocidad(uint8_t porcentaje)
{
    if (porcentaje > 100)
        porcentaje = 100;
    uint32_t ancho = (RELOJ / FREC_PWM) * porcentaje / 100;
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, ancho);
}


/**
 * @brief Ejecuta el precalentamiento del sensor MQ.
 *
 * Bloqueante: espera PREHEAT_TICKS * 50 ms.
 */
void humo_Precalentar(void)
{
    while (preheating < PREHEAT_TICKS)
    {
        SLEEP;
        preheating++;
        if ((preheating % 20) == 0) // cada segundo
        {
            UARTprintf("\033[2K\rPrecalentando sensor de humos... %d s", preheating / 20);
        }
    }
    UARTprintf("\033[2K\rSensor listo. Iniciando deteccion...     \n");
}

/**
 * @brief Lee ADC y pin digital del sensor de humos; actualiza estados.
 *
 * Actualiza: adcValue, gasDetectadoAnag, gasDetected
 */
void humo_Detectar(void)
{
    ADCProcessorTrigger(ADC0_BASE, 3);
    while (!ADCIntStatus(ADC0_BASE, 3, false))
        ;
    ADCIntClear(ADC0_BASE, 3);
    ADCSequenceDataGet(ADC0_BASE, 3, &adcValue);

    /* PP0 DOUT (digital) */
    gasDetected = (GPIOPinRead(GPIO_PORTP_BASE, GPIO_PIN_0) == 0);

    if (adcValue > 870)
        gasDetectadoAnag = 1;
    else
        gasDetectadoAnag = 0;

    UARTprintf("\033[2K\rADC: %u\tDOUT: %s DIGITAL), ESTADO: %s", adcValue,
               gasDetected ? "HIGH (Gas" : "LOW (Clean", gasDetectadoAnag ? "ALARMA GAS DETECTADO" : "AIRE LIMPIO");
}

/**
 * @brief Inicializa ADC0 y pines asociados al sensor de humos.
 */
void humo_Inicializar_Pines(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); // PD2 AIN13
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP); // PP0 DOUT
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH13 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);

    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_0);
    GPIOPadConfigSet(GPIO_PORTP_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
}


/**
 * @brief Inicializa UART0 a 115200 baudios.
 */
void Inicializacion_UART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, RELOJ);
}
//void Estado_uart(void){
//    char buffer_local[40];
//    char buffer_local2[40];
//    sprintf(buffer_local, "LUZ: %.2f  TEMP: %.2f GAS: %u  SOS_GAS:%s ",lux,T_act,adcValue,gasDetectadoAnag ? "ALARMA GAS DETECTADO" : "AIRE LIMPIO");
//    sprintf(buffer_local2, "PUERTA: %d AYUDA: %d EMERGENCIA: %d Comida: %d Pastilla: %d",puerta_abierta,ayuda,emergencia,comida,pastilla);
//    UARTprintf("\n-------------------------------------------------------------------------------------------------------------------------------");
//    UARTprintf("%s", buffer_local);
//    UARTprintf(" %s", buffer_local2);
//    UARTprintf("\n-------------------------------------------------------------------------------------------------------------------------------");
//}

/**
 * @brief Configura reloj del sistema y Timer0A con interrupcion cada 50 ms.
 */
void Reloj_inicializacion(void)
{
    RELOJ = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);
    SysCtlDelay(RELOJ / 3);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ / FREC_TIMER) - 1);
    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer0);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);
}


/**
 * @brief Inicializa OPT3001 y BME280; detecta boosterpack en slot 1 o 2.
 */
void sensores_booster_inic(void)
{
    if (Detecta_BP(1))
    {
        UARTprintf("\n--------------------------------------");
        UARTprintf("\n  BOOSTERPACK detectado en posicion 1");
        UARTprintf("\n   Configurando puerto I2C0");
        UARTprintf("\n--------------------------------------");
        Conf_Boosterpack(1, RELOJ);
    }
    else if (Detecta_BP(2))
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
    Sensor_OK = Test_I2C_Dir(OPT3001_SLAVE_ADDRESS);
    if (!Sensor_OK)
    {
        UARTprintf("Error en OPT3001\n");
        Opt_OK = 0;
    }
    else
    {
        OPT3001_init();
        UARTprintf("Hecho!\n");
        UARTprintf("Leyendo DevID... ");
        DevID = OPT3001_readDeviceId();
        UARTprintf("DevID= 0X%x \n", DevID);
        Opt_OK = 1;
    }

    UARTprintf("Inicializando BME280... ");
    Sensor_OK = Test_I2C_Dir(BME280_I2C_ADDRESS2);
    if (!Sensor_OK)
    {
        UARTprintf("Error en BME280\n");
        Bme_OK = 0;
    }
    else
    {
        bme280_data_readout_template();
        bme280_set_power_mode(BME280_NORMAL_MODE);
        UARTprintf("Hecho! \nLeyendo DevID... ");
        readI2C(BME280_I2C_ADDRESS2, BME280_CHIP_ID_REG, &DevID, 1);
        UARTprintf("DevID= 0X%x \n", DevID);
        Bme_OK = 1;
    }
}

/**
 * @brief Lee sensores de luz y temperatura/humedad y publica por UART.
 *
 * Variables actualizadas: lux, lux_i, T_act
 */
/** @brief Funcion lee_sensores. */
void lee_sensores(void)
{

    char buffer_local[32];
    if (Opt_OK)
    {
        lux = OPT3001_getLux();
        lux_i = (int)round(lux);
        UARTprintf(" OPT300: %d ", lux_i);
    }
    if (Bme_OK)
    {
        returnRslt = bme280_read_pressure_temperature_humidity(&g_u32ActualPress, &g_s32ActualTemp, &g_u32ActualHumity);
        T_act = (float)g_s32ActualTemp / 100.0f;

        sprintf(buffer_local, " Temperatura: %.2f C ", T_act);
        UARTprintf("%s", buffer_local);
    }
}


/**
 * @brief Inicializa servo en PF1 y configura PWM para 50 Hz.
 */
void Servo_inic(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_64);
    GPIOPinConfigure(GPIO_PF1_M0PWM1);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PeriodoPWM = 37499; // 50 Hz @ 1.875 MHz
    pos_180 = 0;
    posicion = Min_pos + ((Max_pos - Min_pos) * pos_180) / 180;

    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, PeriodoPWM);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, posicion);
    PWMGenEnable(PWM0_BASE, PWM_GEN_0);
    PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, true);
}

/**
 * @brief Mueve servo hacia angulo objetivo suavemente.
 * @param angulo_objetivo 0..180
 */
void Servo(int angulo_objetivo)
{
    int paso = 5;
    if (angulo_objetivo > 180)
        angulo_objetivo = 180;
    if (angulo_objetivo < 0)
        angulo_objetivo = 0;

    if (pos_180 < angulo_objetivo)
    {
        pos_180 += paso;
        if (pos_180 > angulo_objetivo)
            pos_180 = angulo_objetivo;
    }
    else if (pos_180 > angulo_objetivo)
    {
        pos_180 -= paso;
        if (pos_180 < angulo_objetivo)
            pos_180 = angulo_objetivo;
    }
    else
        pos_180 = angulo_objetivo;

    posicion = Min_pos + ((Max_pos - Min_pos) * pos_180) / 180;
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, posicion);
}


/**
 * @brief Inicializa la pantalla FT800 por SPI.
 */
void inicia_pantalla(void)
{
    HAL_Init_SPI(1, RELOJ);
    Inicia_pantalla();
    SysCtlDelay(RELOJ / 3);
}

/**
 * @brief Redibuja la pantalla completa (fondo, mensajes, botones).
 */
void dibuja_pantalla(void)
{
    Nueva_pantalla(246, 197, 175);
    DibujaMensaje();
    DibujaRecordatorios(pastilla, comida);
    DibujaBotones();
    Dibuja();
}

/**
 * @brief Anade un mensaje al buffer de pantalla (max 2).
 * @param s cadena a mostrar
 */
/** @brief Funcion MandaMensaje. */
void MandaMensaje(char *s)
{
    cont++;
    if (cont > 2)
    {
        strcpy(buffer[0], buffer[1]);
        strcpy(buffer[1], s);
    }
    else
    {
        strcpy(buffer[cont - 1], s);
    }
}

/**
 * @brief Dibuja los mensajes almacenados en pantalla.
 */
void DibujaMensaje(void)
{
    ComColor(255, 255, 255);
    ComRect(HSIZE / 20, VSIZE / 9, HSIZE / 2, VSIZE / 2, true);
    ComColor(0, 0, 0);
    ComTXT(HSIZE / 20 + 5, VSIZE / 9 + 5, 28, 0, buffer[0]);
    ComTXT(HSIZE / 20 + 5, VSIZE / 3 + 5, 28, 0, buffer[1]);
}

/**
 * @brief Dibuja recordatorios de pastilla y comida.
 * @param pastilla indicador
 * @param comida indicador
 */
void DibujaRecordatorios(int pastilla, int comida)
{
    ComColor(226, 169, 241);
    ComRect(HSIZE * 7 / 13, VSIZE / 9, HSIZE * 8 / 10, VSIZE / 4, true);
    ComRect(HSIZE * 7 / 13, VSIZE / 4 + 30, HSIZE * 8 / 10, VSIZE / 2, true);

    if (pastilla == 0)
        ComColor(226, 169, 241);
    else
        ComColor(0, 0, 0);
    ComTXT(HSIZE * 7 / 12, VSIZE / 9 + 5, 28, 0, "Pastilla");

    if (comida == 0)
        ComColor(226, 169, 241);
    else
        ComColor(0, 0, 0);
    ComTXT(HSIZE * 7 / 12, VSIZE / 4 + 35, 28, 0, "Comida");
}

/**
 * @brief Dibuja los botones graficos de la UI.
 */
void DibujaBotones(void)
{
    ComFgcolor(255, 117, 31);
    ComColor(255, 255, 255);
    Boton(HSIZE / 20, VSIZE / 2 + 20, HSIZE / 5, HSIZE / 5, 28, "Ayuda");

    ComFgcolor(255, 0, 0);
    ComColor(255, 255, 255);
    Boton(HSIZE / 4 + 10, VSIZE / 2 + 20, HSIZE / 5, HSIZE / 5, 28, "SOS");

    if (lampara == 0)
    {
        ComFgcolor(120, 120, 120);
        ComColor(255, 255, 255);
    }
    else
    {
        ComFgcolor(255, 196, 96);
        ComColor(0, 0, 0);
    }
    Boton(HSIZE * 8 / 15, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "LUZ");

    if (ventilador == 0)
    {
        ComFgcolor(120, 120, 120);
        ComColor(255, 255, 255);
    }
    else
    {
        ComFgcolor(65, 162, 216);
        ComColor(0, 0, 0);
    }
    Boton(HSIZE * 3 / 4, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "AIRE");

    if (pastilla == 0)
        ComFgcolor(100, 100, 100);
    else
        ComFgcolor(255, 0, 0);
    Boton(HSIZE * 9 / 11 + 5, VSIZE / 9 - 5, HSIZE / 9, HSIZE / 9, 20, "");

    if (comida == 0)
        ComFgcolor(100, 100, 100);
    else
        ComFgcolor(255, 0, 0);
    Boton(HSIZE * 9 / 11 + 5, VSIZE / 4 + 25, HSIZE / 9, HSIZE / 9, 20, "");
}

/**
 * @brief Procesa la interaccion tactil y actualiza estados.
 *
 * Cambia: lampara, ventilador, pastilla, comida, ayuda, emergencia.
 */
void DatosPantalla(void)
{
    if (Boton(HSIZE * 4 / 5, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "AIRE"))
    {
        SysCtlDelay(10 * MSEC);
        while (Boton(HSIZE * 4 / 5, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "AIRE")); // Debouncing
        SysCtlDelay(10 * MSEC);
        ventilador = !ventilador;
    }

    if (Boton(HSIZE * 7 / 12, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "LUZ"))
    {
        SysCtlDelay(10 * MSEC);
        while (Boton(HSIZE * 7 / 12, VSIZE / 2 + 30, HSIZE / 6 + 5, HSIZE / 6 + 5, 28, "LUZ")); // Debouncing
        SysCtlDelay(10 * MSEC);
        lampara = !lampara;
    }

    if (Boton(HSIZE / 10, VSIZE / 2 + 20, HSIZE / 5, HSIZE / 5, 28, "Ayuda"))
        ayuda = 1;
    if (Boton(HSIZE / 3 + 10, VSIZE / 2 + 20, HSIZE / 5, HSIZE / 5, 28, "SOS"))
        emergencia = 1;
    if (Boton(HSIZE * 7 / 9 + 10, VSIZE / 9, HSIZE / 9, HSIZE / 9, 20, ""))
        pastilla = 0;
    if (Boton(HSIZE * 7 / 9 + 10, VSIZE / 4 + 30, HSIZE / 9, HSIZE / 9, 20, ""))
        comida = 0;
}

