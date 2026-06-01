/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * Descripción: 
 * INTEGRACIÓN FINAL Y COMPLETA
 * - Muestreo exacto a 50Hz (Timer0) para el MAX30102.
 * - Transmisión serial UART cada 2s (Módulo de Jeison Tuquerrez).
 * - Máquina de Estados con 5 LEDs + Buzzer mapeados de RD0 a RD6 (excluyendo RD1).
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"     
#include <stdio.h>    

/* =========================================================
 * 1. ASIGNACIÓN DE PINES (PUERTO D)
 * ========================================================= */
#define LED_ENCENDIDO        LATDbits.LATD0  // RD0 
#define LED_PREPARANDO       LATDbits.LATD2  // RD2 
#define LED_FUNCIONAL        LATDbits.LATD3  // RD3 
#define LED_ESPERA           LATDbits.LATD4  // RD4 
#define LED_ALARMA           LATDbits.LATD5  // RD5 
#define BUZZER               LATDbits.LATD6  // RD6 -> Alarma sonora

#define TRIS_LED_ENCENDIDO   TRISDbits.TRISD0
#define TRIS_LED_PREPARANDO  TRISDbits.TRISD2
#define TRIS_LED_FUNCIONAL   TRISDbits.TRISD3
#define TRIS_LED_ESPERA      TRISDbits.TRISD4
#define TRIS_LED_ALARMA      TRISDbits.TRISD5
#define TRIS_BUZZER          TRISDbits.TRISD6

/* =========================================================
 * 2. UMBRALES MÉDICOS DE REFERENCIA
 * ========================================================= */
#define TEMP_MAX 37.5
#define TEMP_MIN 30.0
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
    /* =========================================================
     * PASO 1 Y 2 DIAGRAMA: INICIO / RESET E INICIALIZACIÓN
     * ========================================================= */
    OSCCON = 0x72; // Configurar oscilador interno a 8MHz
    
    // Configurar pines del Puerto D como salidas digitales (Excepto RD1)
    TRIS_LED_ENCENDIDO = 0;   TRIS_LED_PREPARANDO = 0;
    TRIS_LED_FUNCIONAL = 0;    TRIS_LED_ESPERA = 0;
    TRIS_LED_ALARMA = 0;       TRIS_BUZZER = 0;
    
    // Estados iniciales de los LEDs
    LED_ENCENDIDO = 1;   // Se enciende de inmediato indicando energía
    LED_PREPARANDO = 1;  // Indica que los periféricos se están inicializando
    LED_FUNCIONAL = 0;   LED_ESPERA = 0;   LED_ALARMA = 0;   BUZZER = 0;
    
    // Inicialización física de módulos
    UART_Init();    
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); // El pin RD1 se configura internamente dentro de esta función
    
    /* =========================================================
     * PASO 3 DIAGRAMA: SISTEMA INICIALIZADO
     * ========================================================= */
    __delay_ms(500);     // Breve pausa visual para notar el estado de preparación
    LED_PREPARANDO = 0;  // Apagamos el indicador de inicialización
    
    OLED_String(0, 0, "   PULSIOXIMETRO ");
    OLED_String(2, 0, " Coloque el dedo ");
    OLED_String(4, 0, " Temp: --.-- C   "); 
    
    // Variables de control de sensores
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    unsigned char estado_pantalla = 0; // 0=Sin dedo, 1=Calculando, 2=Estable
    
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    DS18B20_StartConversion(); // Primer disparo del sensor de temperatura
    
    // CONFIGURACIÓN DEL TIMER0 (Interrupción estricta a 50Hz - Cada 20ms)
    T0CON = 0x88; 
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1; 
    INTCONbits.PEIE = 1;   
    INTCONbits.GIE = 1;    

    /* =========================================================
     * PASO 4 DIAGRAMA: BUCLE PRINCIPAL
     * ========================================================= */
    while(1) {
        
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                /* =========================================================
                 * BLOQUE LENTO (CADA 2 SEGUNDOS) - DS18B20 + UART
                 * ========================================================= */
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  
                    contador_muestras_temp = 0; 
                    
                    // 1. Leer temperatura corporal
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        sprintf(buffer_temp, " Temp: %0.2f C   ", temperatura);
                        OLED_String(4, 0, buffer_temp); 
                    } else {
                        OLED_String(4, 0, " Temp: Error     ");
                    }
                    
                    // 2. Transmisión UART condicional
                    if (estado_pantalla == 2) {
                        UART_SendString("--------------------------------------------\r\n");
                        UART_SendString("Temperatura : ");
                        UART_SendFloat(temperatura, 2);
                        UART_SendString(" C");
                        if (temperatura > TEMP_MAX) UART_SendString("  [!] FIEBRE\r\n");
                        else if (temperatura < TEMP_MIN) UART_SendString("  [!] HIPOTERMIA\r\n");
                        else UART_SendString("  [OK]\r\n");
                        
                        UART_SendString("Frec. Pulso : ");
                        UART_SendUInt(lpm);
                        UART_SendString(" BPM");
                        if (lpm > BPM_MAX || lpm < BPM_MIN) UART_SendString("  [!] FUERA DE RANGO\r\n");
                        else UART_SendString("  [OK]\r\n");
                        UART_SendString("--------------------------------------------\r\n\r\n");
                    }
                    
                    // 3. Solicitar nueva conversión asíncrona
                    DS18B20_StartConversion(); 
                }
                
                /* =========================================================
                 * GESTIÓN DE LA MÁQUINA DE ESTADOS POR HARDWARE
                 * ========================================================= */
                if (datos_sensor.red < 15000) {
                    /* --- PASO 7 DIAGRAMA: MODO ESPERA (Sin dedo) --- */
                    LED_FUNCIONAL = 0;
                    LED_ESPERA = 1;
                    LED_ALARMA = 0; 
                    BUZZER = 0;
                    
                    if (estado_pantalla != 0) {
                        OLED_String(2, 0, " Coloque el dedo ");
                        estado_pantalla = 0;
                    }
                    lpm = 0; 
                    last_lpm = 0;
                } 
                else {
                    /* --- PASO 8 DIAGRAMA: MODO FUNCIONAL (Dedo detectado) --- */
                    LED_ESPERA = 0;
                    LED_FUNCIONAL = 1;
                    
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (lpm > 0) {
                        /* --- PASO 10, 11 Y 12 DIAGRAMA: EVALUACIÓN DE ALARMAS --- */
                        if (temperatura > TEMP_MAX || temperatura < TEMP_MIN || lpm > BPM_MAX || lpm < BPM_MIN) {
                            // ESTADO ALARMA
                            LED_ALARMA = 1;
                            BUZZER = 1;
                        } else {
                            // ESTADO NORMAL
                            LED_ALARMA = 0;
                            BUZZER = 0;
                        }
                        
                        // Actualización dinámica de la pantalla OLED
                        if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                            sprintf(buffer_texto, " LPM: %-3u        ", lpm);
                            OLED_String(2, 0, buffer_texto);
                            estado_pantalla = 2;
                            last_lpm = lpm;
                        }
                    } else {
                        // Estado intermedio: Dedo puesto pero filtrando
                        if (estado_pantalla != 1) {
                            OLED_String(2, 0, " Calculando...   ");
                            estado_pantalla = 1;
                        }
                        // Evitamos alarmas falsas por BPM en 0 durante el cálculo inicial
                        LED_ALARMA = 0;
                        BUZZER = 0;
                    }
                }
            }
        }
    }
}