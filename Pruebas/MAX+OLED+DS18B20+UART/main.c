/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * Descripción: 
 * Prueba de Integración: Timer0 + MAX30102 + DS18B20 + OLED + UART.
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"  
#include <stdio.h>    

// Umbrales médicos para el diagnóstico por UART
#define TEMP_MAX 37.5
#define TEMP_MIN 35.0
#define BPM_MAX 100
#define BPM_MIN 60

/* BANDERA VOLÁTIL DE INTERRUPCIÓN */
volatile unsigned char flag_nueva_muestra = 0;

/* RUTINA DE SERVICIO DE INTERRUPCIÓN (ISR) - Timer0 (Cada 20ms) */
void __interrupt() ISR(void) {
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;  
        TMR0H = 0x63; 
        TMR0L = 0xC0; 
        flag_nueva_muestra = 1;  
    }
}

void main(void) {
    // 1. INICIALIZACIÓN DEL MICROCONTROLADOR
    OSCCON = 0x72; // 8MHz
    
    // 2. INICIALIZACIÓN DE PERIFÉRICOS
    UART_Init();    // Inicializamos el módulo UART a 9600 baudios
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); 
    
    OLED_String(0, 0, "  PULSIOXIMETRO ");
    OLED_String(2, 0, "Coloque el dedo ");
    OLED_String(4, 0, "Temp: --.-- C   "); 
    
    // Variables de sensores
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    unsigned char estado_pantalla = 0; // 0=Sin dedo, 1=Calculando, 2=Funcional
    
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    DS18B20_StartConversion(); 
    
    // CONFIGURACIÓN DEL TIMER0
    T0CON = 0x88; 
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1; 
    INTCONbits.PEIE = 1;   
    INTCONbits.GIE = 1;    

    // 4. BUCLE PRINCIPAL
    while(1) {
        
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                /* =========================================================
                 * BLOQUE LENTO (EJECUTADO CADA 2 SEGUNDOS)
                 * DS18B20 + TRANSMISIÓN UART
                 * ========================================================= */
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  
                    contador_muestras_temp = 0; 
                    
                    // 1. Leer temperatura y actualizar OLED
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        sprintf(buffer_temp, "Temp: %0.2f C   ", temperatura);
                        OLED_String(4, 0, buffer_temp); 
                    } else {
                        OLED_String(4, 0, "Temp: Error     ");
                    }
                    
                    // 2. TRANSMISIÓN UART (Solo transmitimos si el dedo está puesto)
                    if (estado_pantalla == 2) {
                        UART_SendString("--------------------------------------------\r\n");
                        
                        // Temperatura
                        UART_SendString("Temperatura : ");
                        UART_SendFloat(temperatura, 2);
                        UART_SendString(" C");
                        if (temperatura > TEMP_MAX) UART_SendString("  [!] FIEBRE\r\n");
                        else if (temperatura < TEMP_MIN) UART_SendString("  [!] HIPOTERMIA\r\n");
                        else UART_SendString("  [OK]\r\n");
                        
                        // BPM
                        UART_SendString("Frec. Pulso : ");
                        UART_SendUInt(lpm);
                        UART_SendString(" BPM");
                        if (lpm > BPM_MAX || lpm < BPM_MIN) UART_SendString("  [!] FUERA DE RANGO\r\n");
                        else UART_SendString("  [OK]\r\n");
                        
                        UART_SendString("--------------------------------------------\r\n\r\n");
                    }
                    
                    // 3. Pedir siguiente temperatura
                    DS18B20_StartConversion(); 
                }
                
                /* =========================================================
                 * BLOQUE RÁPIDO (MAX30102 + OLED) - 50 veces por segundo
                 * ========================================================= */
                if (datos_sensor.red < 15000) {
                    if (estado_pantalla != 0) {
                        OLED_String(2, 0, "Coloque el dedo ");
                        estado_pantalla = 0;
                    }
                    lpm = 0; 
                    last_lpm = 0;
                } 
                else {
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (lpm > 0) {
                        if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                            sprintf(buffer_texto, "LPM: %-3u        ", lpm);
                            OLED_String(2, 0, buffer_texto);
                            estado_pantalla = 2;
                            last_lpm = lpm;
                        }
                    } else {
                        if (estado_pantalla != 1) {
                            OLED_String(2, 0, "Calculando...   ");
                            estado_pantalla = 1;
                        }
                    }
                }
            }
        }
    }
}