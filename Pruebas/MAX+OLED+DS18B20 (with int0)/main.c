/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * Descripción: 
 * Integración con Interrupción por Timer0.
 * Integración de sensores MAX30102 (I2C) y DS18B20 (1-Wire) con pantalla OLED.
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include <stdio.h>    

/* =========================================================
 * 1. BANDERA VOLÁTIL DE INTERRUPCIÓN
 * ========================================================= */
volatile unsigned char flag_nueva_muestra = 0;

/* =========================================================
 * 2. RUTINA DE SERVICIO DE INTERRUPCIÓN (ISR) - Timer0
 * ========================================================= */
void __interrupt() ISR(void) {
    // Si la interrupción fue por el desbordamiento del Timer0 
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;  // 1. Limpiamos la bandera del hardware
        
        // 2. Recargamos el Timer0 para los próximos 20ms (a 8MHz)
        // (65536 - 40000 ciclos = 25536 -> 0x63C0)
        TMR0H = 0x63; 
        TMR0L = 0xC0; 
        
        // 3. Levantamos nuestra bandera de software
        flag_nueva_muestra = 1;  
    }
}

void main(void) {
    // INICIALIZACIÓN DEL MICROCONTROLADOR
    OSCCON = 0x72; // 8MHz
    
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); 
    
    OLED_String(0, 0, "   PULSIOXIMETRO ");
    OLED_String(2, 0, " Coloque el dedo ");
    OLED_String(4, 0, " Temp: --.-- C   "); 
    
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    unsigned char estado_pantalla = 0; 
    
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    DS18B20_StartConversion(); 
    
    /* =========================================================
     * 3. CONFIGURACIÓN DEL TIMER0 PARA INTERRUPCIONES
     * ========================================================= */
    // T0CON: Timer0 ON, 16-bits, Reloj Interno, Sin Prescaler
    T0CON = 0x88; 
    
    // Carga inicial para 20ms
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    
    INTCONbits.TMR0IF = 0; // Limpia la bandera por si acaso
    INTCONbits.TMR0IE = 1; // Habilita la interrupción del Timer0
    INTCONbits.PEIE = 1;   // Habilita interrupciones periféricas
    INTCONbits.GIE = 1;    // Habilita interrupciones globales
    
    /* =========================================================
     *   4. BUCLE PRINCIPAL
     * ========================================================= */
    while(1) {
        
        // Solo entramos aquí exactamente cada 20 milisegundos
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; // Bajamos la bandera al instante
            
            // Leemos el sensor 
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                /* --- MÓDULO DS18B20 (Jeison Tuquerrez) --- */
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  // Pasaron 100 interrupciones (2 seg exactos)
                    contador_muestras_temp = 0; 
                    
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        sprintf(buffer_temp, " Temp: %0.2f C   ", temperatura);
                        OLED_String(4, 0, buffer_temp); 
                    } else {
                        OLED_String(4, 0, " Temp: Error     ");
                    }
                    DS18B20_StartConversion(); 
                }
                
                /* --- LÓGICA DEL MAX30102 Y OLED --- */
                if (datos_sensor.red < 15000) {
                    if (estado_pantalla != 0) {
                        OLED_String(2, 0, " Coloque el dedo ");
                        estado_pantalla = 0;
                    }
                    lpm = 0; 
                    last_lpm = 0;
                } 
                else {
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (lpm > 0) {
                        if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                            sprintf(buffer_texto, " LPM: %-3u        ", lpm);
                            OLED_String(2, 0, buffer_texto);
                            estado_pantalla = 2;
                            last_lpm = lpm;
                        }
                    } else {
                        if (estado_pantalla != 1) {
                            OLED_String(2, 0, " Calculando...   ");
                            estado_pantalla = 1;
                        }
                    }
                }
            }
        }
    }
}