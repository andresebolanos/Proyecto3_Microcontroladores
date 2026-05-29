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
    char buffer_texto[16];
    char buffer_debug[16]; // Buffer para ver el valor real del sensor
    
    while(1) {
        if (MAX30102_ReadSample(&datos_sensor)) {
            
            /* 1. IMPRIMIR VALOR RAW PARA DIAGNÓSTICO */
            sprintf(buffer_debug, "RAW: %-6lu     ", datos_sensor.red);
            OLED_String(4, 0, buffer_debug);
            
            /* 2. CONTROL DE MENSAGES BASADO EN EL HARDWARE */
            // Si el valor RAW es muy bajo, significa que físicamente NO hay un dedo tapando el LED
            if (datos_sensor.red < 15000) {
                OLED_String(2, 0, "Coloque el dedo ");
                lpm = 0; // Reiniciamos la variable local
            } 
            else {
                // Si el dedo está puesto, procesamos el algoritmo
                MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                
                // Solo mostramos los LPM si el algoritmo ya logró calcular un valor mayor a cero
                if (lpm > 0) {
                    sprintf(buffer_texto, "LPM: %-3u        ", lpm);
                    OLED_String(2, 0, buffer_texto);
                } else {
                    // Si el dedo está puesto pero el filtro se está estabilizando
                    OLED_String(2, 0, "Calculando...   ");
                }
            }
        }
        __delay_ms(2); 
    }
}