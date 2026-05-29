/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * * Descripción: 
 * Integración de sensores MAX30102 (I2C) y DS18B20 (1-Wire) con pantalla OLED.
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  // Librería de temperatura desarrollada por Jeison Tuquerrez
#include <stdio.h>    // Necesario para la función sprintf

void main(void) {
    // 1. INICIALIZACIÓN DEL MICROCONTROLADOR
    OSCCON = 0x72; // Configura el oscilador interno a 8MHz
    
    // 2. INICIALIZACIÓN DE PERIFÉRICOS Y SENSORES
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); 
    
    // 3. INTERFAZ GRÁFICA INICIAL
    OLED_String(0, 0, "  PULSIOXIMETRO ");
    OLED_String(2, 0, "Coloque el dedo ");
    OLED_String(4, 0, "Temp: --.-- C   "); 
    
    // Variables para el sensor de pulso (MAX30102)
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    
    // Control de estado de la pantalla 
    // 0 = Sin dedo, 1 = Calculando, 2 = Mostrando LPM
    unsigned char estado_pantalla = 0; 
    
    // Variables para el sensor de temperatura (DS18B20)
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    
    // Buffers de texto para formatear los datos a mostrar en la OLED
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    // Solicitamos la primera conversión de temperatura antes de entrar al bucle
    DS18B20_StartConversion(); 
    
    // 4. BUCLE PRINCIPAL
    while(1) {
        // La lectura del MAX30102 dicta el ritmo del bucle (~50Hz)
        if (MAX30102_ReadSample(&datos_sensor)) {
            
            /* =========================================================
             * MÓDULO DS18B20: CONTROL ASÍNCRONO 
             * (Librería por Jeison Tuquerrez)
             * =========================================================
             * Se lee la temperatura cada 100 muestras del MAX30102 (~2 seg).
             * Esto evita usar delays que arruinarían el muestreo del pulso.
             */
            contador_muestras_temp++;
            if (contador_muestras_temp >= 100) { 
                contador_muestras_temp = 0; // Reinicia el contador de ciclos
                
                // Lee el resultado de la conversión que se hizo en segundo plano
                if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                    sprintf(buffer_temp, "Temp: %0.2f C   ", temperatura);
                    OLED_String(4, 0, buffer_temp); 
                } else {
                    OLED_String(4, 0, "Temp: Error     ");
                }
                
                // Ordena una NUEVA conversión y continúa inmediatamente
                DS18B20_StartConversion();
            }
            /* ========================================================= */
            
            
            /* =========================================================
             * MÓDULO MAX30102: MÁQUINA DE ESTADOS Y PROCESAMIENTO
             * ========================================================= */
            if (datos_sensor.red < 15000) {
                // ESTADO: Dedo no detectado
                if (estado_pantalla != 0) {
                    OLED_String(2, 0, "Coloque el dedo ");
                    estado_pantalla = 0;
                }
                lpm = 0; 
                last_lpm = 0;
            } 
            else {
                // ESTADO: Dedo detectado -> Procesar algoritmo
                unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                
                if (lpm > 0) {
                    // Actualiza la OLED solo si hay un nuevo latido o cambió el valor de LPM
                    if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                        sprintf(buffer_texto, "LPM: %-3u        ", lpm);
                        OLED_String(2, 0, buffer_texto);
                        estado_pantalla = 2;
                        last_lpm = lpm;
                    }
                } else {
                    // ESTADO: Estabilizando filtro IIR (Calculando)
                    if (estado_pantalla != 1) {
                        OLED_String(2, 0, "Calculando...   ");
                        estado_pantalla = 1;
                    }
                }
            }
        }
    }
}