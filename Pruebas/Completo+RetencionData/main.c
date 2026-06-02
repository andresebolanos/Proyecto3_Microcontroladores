/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * Descripción: 
 * - Mapeo de pines seguro (RD1 libre para DS18B20).
 * - Implementación de FUNCIÓN DATA-HOLD en Modo Espera.
 * - Retiene el último registro médico válido en pantalla al quitar el dedo.
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"     
#include <stdio.h>    

/* =========================================================
 * 1. ASIGNACIÓN DE PINES (PUERTO D) - RD1 LIBRE PARA DS18B20
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

#define TEMP_MAX 37.5
#define TEMP_MIN 35.0
#define BPM_MAX 100
#define BPM_MIN 60

volatile unsigned char flag_nueva_muestra = 0;

void __interrupt() ISR(void) {
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;  
        TMR0H = 0x63; 
        TMR0L = 0xC0; 
        flag_nueva_muestra = 1;  
    }
}

void main(void) {
    OSCCON = 0x72; // 8MHz
    
    TRIS_LED_ENCENDIDO = 0;   TRIS_LED_PREPARANDO = 0;
    TRIS_LED_FUNCIONAL = 0;    TRIS_LED_ESPERA = 0;
    TRIS_LED_ALARMA = 0;       TRIS_BUZZER = 0;
    
    LED_ENCENDIDO = 1;   LED_PREPARANDO = 1;  
    LED_FUNCIONAL = 0;   LED_ESPERA = 0;   LED_ALARMA = 0;   BUZZER = 0;
    
    UART_Init();    
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); 
    
    __delay_ms(500);     
    LED_PREPARANDO = 0;  
    
    OLED_String(0, 0, "  PULSIOXIMETRO ");
    OLED_String(2, 0, "Coloque el dedo ");
    OLED_String(4, 0, "Temp: --.-- C   "); 
    
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    unsigned char estado_pantalla = 0; // 0=Espera, 1=Calculando, 2=Estable
    
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    /* VARIABLES DE HISTORIAL (DATA-HOLD) */
    unsigned char ultimo_lpm = 0;
    float ultima_temp = 0.0;
    unsigned char tiene_registro = 0; // Bandera de memoria
    
    DS18B20_StartConversion(); 
    
    T0CON = 0x88; 
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1; 
    INTCONbits.PEIE = 1;   
    INTCONbits.GIE = 1;    

    while(1) {
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                /* =========================================================
                 * BLOQUE LENTO (CADA 2 SEGUNDOS) - LECTURA DS18B20
                 * ========================================================= */
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  
                    contador_muestras_temp = 0; 
                    
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        if (!(estado_pantalla == 0 && tiene_registro == 1)) {
                            sprintf(buffer_temp, "Temp: %0.2f C   ", temperatura);
                            OLED_String(4, 0, buffer_temp); 
                        }
                    }
                    
                    // Reporte UART (Solo si el paciente está activamente conectado)
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
                    
                    DS18B20_StartConversion(); 
                }
                
                /* =========================================================
                 * GESTIÓN DE LA MÁQUINA DE ESTADOS Y PANTALLA
                 * ========================================================= */
                if (datos_sensor.red < 15000) {
                    /* --- MODO ESPERA (Sin dedo) --- */
                    LED_FUNCIONAL = 0;
                    LED_ESPERA = 1;
                    LED_ALARMA = 0; 
                    BUZZER = 0;
                    
                    if (estado_pantalla != 0) {
                        estado_pantalla = 0;
                        
                        // LÓGICA DATA-HOLD: ¿Había un paciente registrado antes?
                        if (tiene_registro == 1) {
                            OLED_String(0, 0, "COLOQUE EL DEDO "); // Invitación en la parte superior
                            sprintf(buffer_texto, "LPM Ult: %-3u   ", ultimo_lpm);
                            OLED_String(2, 0, buffer_texto);
                            sprintf(buffer_temp, "Tmp Ult: %0.2f C", ultima_temp);
                            OLED_String(4, 0, buffer_temp);
                        } else {
                            // Pantalla por defecto si nunca se ha colocado un dedo desde el encendido
                            OLED_String(0, 0, "  PULSIOXIMETRO ");
                            OLED_String(2, 0, "Coloque el dedo ");
                            OLED_String(4, 0, "Temp: --.-- C   ");
                        }
                    }
                    lpm = 0; 
                    last_lpm = 0;
                } 
                else {
                    /* --- MODO FUNCIONAL (Dedo detectado) --- */
                    LED_ESPERA = 0;
                    LED_FUNCIONAL = 1;
                    
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (lpm > 0) {
                        // Evaluación de Alarmas
                        if (temperatura > TEMP_MAX || temperatura < TEMP_MIN || lpm > BPM_MAX || lpm < BPM_MIN) {
                            LED_ALARMA = 1;  BUZZER = 1;
                        } else {
                            LED_ALARMA = 0;  BUZZER = 0;
                        }
                        
                        // Actualización dinámica en pantalla
                        if (estado_pantalla != 2 || latido_real || lpm != last_lpm) {
                            OLED_String(0, 0, "  PULSIOXIMETRO "); // Restauramos título original
                            sprintf(buffer_texto, "LPM: %-3u        ", lpm);
                            OLED_String(2, 0, buffer_texto);
                            estado_pantalla = 2;
                            last_lpm = lpm;
                            
                            // RESPALDO CONTINUO EN MEMORIA
                            ultimo_lpm = lpm;
                            if (temperatura > 20.0) { // Validar que sea una lectura real y no el 0.0 inicial
                                ultima_temp = temperatura;
                            }
                            tiene_registro = 1; // Activamos la memoria permanentemente
                        }
                    } else {
                        // Estado de cálculo intermedio
                        if (estado_pantalla != 1) {
                            OLED_String(0, 0, "  PULSIOXIMETRO ");
                            OLED_String(2, 0, "Calculando...   ");
                            estado_pantalla = 1;
                        }
                        LED_ALARMA = 0;  BUZZER = 0;
                    }
                }
            }
        }
    }
}