void Servo(int ang);


//------------------FUNCIONES
// En ang se da el angulo objetivo del servo
// La funcion esta hecha para subir y bajar, usar solo los estados que interesen
void Servo(int ang) {

    switch(estado_servo){
    case 0:
        if(ang!=0)      //Si angulo objetivo distinto de cero iniciar movimiento
            estado_servo=1;
        break;
    case 1:
        if (ang_real < ang){  // Aumentar hasta alcanzar angulo objetivo
            ang_real++;
            posicion=ang_real*16+1300;      // Relacion entre servo real y servo dibujado
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, posicion);
        }
        else {
            estado_servo = 2; // Angulo alcanzado
            t_esp = 0;
        }
        break;
    case 2: // Estado de espera
        t_esp++;
        if (t_esp >= 150) { // 3s
            t_esp = 0;
            estado_servo = 3;
        }
        break;
    case 3:
        if (ang_real > 0) { // Disminnuir hasta alcanzar angulo cero
            ang_real--;
            posicion=ang_real*16+1300;      // Relacion entre servo real y servo dibujado
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, posicion);
        }
        else{  // Vaciado completado, volver al reposo
            ang_real=0;
            piezas[estado-1]=0;
            cajas[estado-1]++;
            sprintf(nuevo_mensaje, "FIN VACIADO");
            estado=0;
            Actualizar_mensaje();
            estado_servo=0;
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, Min_pos);
        }
        break;
    }
    PintarServo();
}

void PintarServo(void){
    // Dibujo siempre con ang_real
    float rad = (-ang_real+90) * M_PI / 180.0f;
    int x2 = (int)(40 * cosf(rad));
    int y2 = (int)(40 * sinf(rad));
    ComLine(HSIZE / 2 + 50, 180, HSIZE / 2 + 50 + x2, 180 + y2, 10);
    ComColor(255, 255, 255);
    ComCirculo(HSIZE / 2 + 50, 180, 15);
}

