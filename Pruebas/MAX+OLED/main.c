#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include <stdio.h> // Necesario para sprintf

void main(void) {
    OSCCON = 0x72; // 8MHz interno
    
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    
    OLED_String(0, 0, "  PULSIOXIMETRO ");
    OLED_String(2, 0, "Coloque el dedo ");
    
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    
    // Variable de control para saber qué hay en pantalla y evitar re-escrituras molestas
    // 0 = Sin dedo, 1 = Calculando, 2 = Mostrando LPM
    unsigned char estado_pantalla = 0; 
    unsigned int raw_counter = 0;
    
    char buffer_texto[16];
    char buffer_debug[16]; 
    
    while(1) {
        if (MAX30102_ReadSample(&datos_sensor)) {
            
            /* 1. IMPRIMIR VALOR RAW CONTROLADO (Cada 15 muestras ~ 3 veces por segundo) */
            /* Esto reduce drásticamente el uso del bus I2C y da prioridad al sensor */
            raw_counter++;
            if (raw_counter >= 15) {
                sprintf(buffer_debug, "RAW: %-6lu     ", datos_sensor.red);
                OLED_String(4, 0, buffer_debug);
                raw_counter = 0;
            }
            
            /* 2. MÁQUINA DE ESTADOS PARA LA PANTALLA */
            if (datos_sensor.red < 15000) {
                // Solo escribe en pantalla si antes estábamos en otro estado
                if (estado_pantalla != 0) {
                    OLED_String(2, 0, "Coloque el dedo ");
                    estado_pantalla = 0;
                }
                lpm = 0; 
                last_lpm = 0;
            } 
            else {
                // Procesamos el algoritmo. 'latido_real' será 1 solo en el instante del pulso.
                unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                
                if (lpm > 0) {
                    // Actualiza la pantalla SÓLO si cambió el valor de LPM, si hay un nuevo latido o si veníamos de otro estado
                    if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                        sprintf(buffer_texto, "LPM: %-3u        ", lpm);
                        OLED_String(2, 0, buffer_texto);
                        estado_pantalla = 2;
                        last_lpm = lpm;
                    }
                } else {
                    // Si el dedo está puesto pero el filtro se está estabilizando rápidamente
                    if (estado_pantalla != 1) {
                        OLED_String(2, 0, "Calculando...   ");
                        estado_pantalla = 1;
                    }
                }
            }
        }
        __delay_ms(2); 
    }
}