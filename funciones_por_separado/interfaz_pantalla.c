/*
 * main.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "driverlib2.h"
#include <stdlib.h>
#include <string.h>
#include "utils/uartstdio.h"
#include "FT800_TIVA.h"

// =======================================================================
// Function Declarations
// =======================================================================
#define dword long
#define byte char
#define MSEC 40000 //Valor para 1ms con SysCtlDelay() 120MHz/40000*3 del delay=1ms
// =======================================================================
// Variable Declarations
// =======================================================================
unsigned long REG_TT[6];
const int32_t REG_CAL[6]= {CAL_DEFAULTS};


#define NUM_SSI_DATA            3

int RELOJ;

int cont; //contador de mensaje
char buffer[2][20];//mensajes a escribir
//valores de cominucacion--------------------------------
int lampara=1;
int ventilador=1;
int pastilla=1;
int comida=1;
int ayuda=0;
int emergencia=0;
//funciones pantallas------------------------------------
void inicia_pantalla(void);
void dibuja_pantalla(void);
void MandaMensaje(char *s);
void DibujaMensaje(void);
void DibujaRecordatorios(int pastilla, int comida);
void DibujaBotones(void);
void DatosPantalla(void);
//---------------------------------------------------------
int main(void)
{

    int i;
    RELOJ=SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);
    inicia_pantalla();

    // ================================================================================================================
    // PANTALLA INICIAL
    // ================================================================================================================
    dibuja_pantalla();
    Espera_pant();
    for(i=0;i<6;i++)    Esc_Reg(REG_TOUCH_TRANSFORM_A+4*i, REG_CAL[i]);



    while(1){
        DatosPantalla();
        MandaMensaje("mensaje 1");
        MandaMensaje("mensaje 2");
        dibuja_pantalla();
    }


}
void inicia_pantalla(void)
{
    HAL_Init_SPI(1, RELOJ);  //Boosterpack a usar, Velocidad del MC
    Inicia_pantalla();
    // =======================================================================
    // Delay before we begin to display anything
    // =======================================================================

    SysCtlDelay(RELOJ/3);
}
void dibuja_pantalla(void)
{
    Nueva_pantalla(246,197,175); //color naranja claro
    DibujaMensaje(); //dibujar cuadro de texto y mensajes (2 últimos)
    DibujaRecordatorios(pastilla,comida); //dibuja mensaje de pastilla y de comida
    DibujaBotones();
    Dibuja();
}
void MandaMensaje(char *s)
{
    cont++;
    if(cont>2)
    {
        strcpy(buffer[0], buffer[1]);
        strcpy(buffer[1], s);
    }
    else
        strcpy(buffer[cont-1], s);
}
void DibujaMensaje(void)
{
    //cuadrado para escibir mensajes
    ComColor(255,255,255);
    ComRect(HSIZE/20,VSIZE/9,HSIZE/2,VSIZE/2, true);
    //Escribir en la pantalla de mensajes
    ComColor(0, 0, 0);
    ComTXT(HSIZE/20+5,VSIZE/9+5, 28, 0,buffer[0]);
    ComTXT(HSIZE/20+5,VSIZE/3+5, 28,0,buffer[1]);
}
void DibujaRecordatorios(int pastilla, int comida)
{
    //cuadrado para escibir mensajes
    ComColor(226, 169, 241);
    ComRect(HSIZE*7/13,VSIZE/9,HSIZE*8/10,VSIZE/4, true);
    ComRect(HSIZE*7/13,VSIZE/4+30,HSIZE*8/10,VSIZE/2, true);
    //texto

    if(pastilla==0)  ComColor(226, 169, 241);
    else ComColor(0, 0, 0);
    ComTXT(HSIZE*7/12,VSIZE/9+5, 28, 0,"Pastilla");

    if(comida==0)  ComColor(226, 169, 241);
    else ComColor(0, 0, 0);
    ComTXT(HSIZE*7/12,VSIZE/4+35, 28,0,"Comida");



}
void DibujaBotones(void)
{
    //boton ayuda
    ComFgcolor(255, 117, 31);
    ComColor(255,255,255);
    Boton(HSIZE/20, VSIZE/2+20,HSIZE/5,HSIZE/5, 28, "Ayuda");

    //boton emergencia
    ComFgcolor(255,0,0);
    ComColor(255,255,255);
    Boton(HSIZE/4+10, VSIZE/2+20,HSIZE/5,HSIZE/5, 28, "SOS");

    //boton luz
    if(lampara==0){
        ComFgcolor(120,120,120);
        ComColor(255,255,255);
    }
    else{
        ComFgcolor(255,196,96);
        ComColor(0,0,0);
    }
    Boton(HSIZE*8/15, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "LUZ");

    //boton ventilador
    if(ventilador==0){
        ComFgcolor(120,120,120);
        ComColor(255,255,255);
    }
    else{
        ComFgcolor(65,162,216);
        ComColor(0,0,0);
    }
    Boton(HSIZE*3/4, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "AIRE");

    //botones recordatorio:
    //checks de comida y pastilla
    if(pastilla==0) ComFgcolor(100, 100, 100);
    else ComFgcolor(255,0,0);
    Boton(HSIZE*9/11+5,VSIZE/9-5,HSIZE/9,HSIZE/9,20, "");
    if(comida==0) ComFgcolor(100,100,100);
    else ComFgcolor(255,0, 0);
    Boton(HSIZE*9/11+5,VSIZE/4+25,HSIZE/9,HSIZE/9,20, "");
}

void DatosPantalla(void){
    if(Boton(HSIZE*4/5, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "AIRE"))
    {  SysCtlDelay(10*MSEC);
       while(Boton(HSIZE*4/5, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "AIRE"));  //Debouncing...
       SysCtlDelay(10*MSEC);
        ventilador=!ventilador;
    }
    if(Boton(HSIZE*7/12, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "LUZ"))
    {
        SysCtlDelay(10*MSEC);
        while(Boton(HSIZE*7/12, VSIZE/2+30,HSIZE/6+5,HSIZE/6+5, 28, "LUZ"));  //Debouncing...
        SysCtlDelay(10*MSEC);
        lampara=!lampara;
    }
    if(Boton(HSIZE/10, VSIZE/2+20,HSIZE/5,HSIZE/5, 28, "Ayuda"))
        ayuda=1;
    if(Boton(HSIZE/3+10, VSIZE/2+20,HSIZE/5,HSIZE/5, 28, "SOS"))
        emergencia=1;
    if(Boton(HSIZE*7/9+10,VSIZE/9,HSIZE/9,HSIZE/9,20, ""))
        pastilla=0;
    if(Boton(HSIZE*7/9+10,VSIZE/4+30,HSIZE/9,HSIZE/9,20, ""))
        comida=0;

}


