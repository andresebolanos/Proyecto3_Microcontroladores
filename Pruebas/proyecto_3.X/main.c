/*
 * Archivo: main.c
 * Proyecto: Monitor Portátil de Signos Vitales - PIC18F4550
 * INTEGRACIÓN FINAL COMPLETA - REFRESCO OPTIMIZADO Y RETENCIÓN DE DATOS
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"     
#include <stdio.h>    

/* =========================================================
 * 1. ASIGNACIÓN COMPLETA DE PINES (PUERTO D y PUERTO B)
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

#define TRIS_BOTON_SLEEP     TRISBbits.TRISB2 // Botón INT2

#define TEMP_MAX 37.5
#define TEMP_MIN 32.0  // Umbral donde se considera temperatura humana válida
#define BPM_MAX 100
#define BPM_MIN 60

/* BANDERAS VOLÁTILES DE INTERRUPCIÓN */
volatile unsigned char flag_nueva_muestra = 0;
volatile unsigned char flag_cambiar_power = 0; 

/* RUTINA DE SERVICIO DE INTERRUPCIÓN (ISR) */
void __interrupt() ISR(void) {
    // 1. Interrupción de Timer0 (Muestreo 50Hz)
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;  
        TMR0H = 0x63; 
        TMR0L = 0xC0; 
        flag_nueva_muestra = 1;  
    }
    
    // 2. Interrupción Externa INT2 (Botón en RB2 presionado)
    if (INTCON3bits.INT2IF) {
        INTCON3bits.INT2IF = 0;         // Limpiar bandera de interrupción INT2
        flag_cambiar_power = 1;         // Avisar al bucle principal
    }
}

/* FUNCION PARA APAGAR PERIFÉRICOS ANTES DEL SLEEP MODE */
void Apagar_Sistema_Completo(void) {
    LATD = 0x00;         // Apagar todos los LEDs y Buzzer
    OLED_Clear();        // Limpiar pantalla 
    MAX30102_Shutdown(); // Apagar diodos internos del oxímetro
}

void main(void) {
    OSCCON = 0x72; // Configurar oscilador interno a 8MHz
    
    // Configurar pines de LEDs y Buzzer como salidas
    TRIS_LED_ENCENDIDO = 0;   TRIS_LED_PREPARANDO = 0;
    TRIS_LED_FUNCIONAL = 0;    TRIS_LED_ESPERA = 0;
    TRIS_LED_ALARMA = 0;       TRIS_BUZZER = 0;
    
    // Configurar RB2 (INT2) como entrada digital para el botón
    TRIS_BOTON_SLEEP = 1;
    INTCON2bits.INTEDG2 = 0;   // Interrupción por flanco de bajada para INT2 (Presionar a GND)
    INTCON3bits.INT2IF = 0;     // Limpiar bandera inicial de INT2
    INTCON3bits.INT2IE = 1;     // Habilitar la interrupción externa INT2
    
    // Estado inicial de arranque
    LED_ENCENDIDO = 1;   LED_PREPARANDO = 1;  
    LED_FUNCIONAL = 0;   LED_ESPERA = 0;   LED_ALARMA = 0;   BUZZER = 0;
    
    // Inicialización de hardware
    UART_Init();    
    I2C_Init();
    OLED_Init();
    MAX30102_Init();
    DS18B20_Init(); 
    
    /* Pantalla de bienvenida */
    OLED_Clear();
    OLED_String(0, 0, "MONITOR DE");
    OLED_String(2, 0, "SIGNOS VITALES");
    OLED_String(6, 0, "Inicializando");

    __delay_ms(2500);

    /* Sensores listos */
    LED_PREPARANDO = 0;

    OLED_Clear();
    OLED_String(0, 0, "MONITOR CARDIACO");
    OLED_String(3, 0, "Coloque el dedo");
    OLED_String(5, 0, "en el sensor");
    
    MAX30102_Sample datos_sensor;
    unsigned char lpm = 0;
    unsigned char last_lpm = 0;
    unsigned char estado_pantalla = 0; 
    
    float temperatura = 0.0;
    unsigned int contador_muestras_temp = 0; 
    char buffer_texto[16];
    char buffer_temp[16]; 
    
    /* VARIABLES DE HISTORIAL (DATA-HOLD) */
    unsigned char ultimo_lpm = 0;
    float ultima_temp = 0.0;
    unsigned char tiene_registro = 0; 
    unsigned char sistema_dormido = 0; 
    
    DS18B20_StartConversion(); 
    
    // CONFIGURACIÓN TIMER0
    T0CON = 0x88; 
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1; 
    INTCONbits.PEIE = 1;   
    INTCONbits.GIE = 1;    

    while(1) {
        
        /* GESTIÓN DE ENERGÍA: PETICIÓN DE DORMIR O DESPERTAR */
        if (flag_cambiar_power == 1) {
            flag_cambiar_power = 0;
            __delay_ms(200);
            
            if (sistema_dormido == 0) {
                sistema_dormido = 1;
                Apagar_Sistema_Completo();
                
                SLEEP(); // El micro se duerme aquí
                NOP();   
            } else {
                sistema_dormido = 0;
                
                // Descongelar oscilador y re-inicializar buses esenciales
                OSCCON = 0x72; 
                I2C_Init();
                OLED_Init();
                MAX30102_Init();
                
                LED_ENCENDIDO = 1;
                estado_pantalla = 99; // Fuerza refresco total de pantalla al despertar
                flag_nueva_muestra = 0;
                DS18B20_StartConversion();
            }
        }

        if (sistema_dormido == 1) {
            continue; 
        }

        /* BUCLE DE MUESTREO NORMAL Y MÁQUINA DE ESTADOS */
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                // BLOQUE LENTO (CADA 2 SEGUNDOS) - LECTURA DS18B20 + UART
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  
                    contador_muestras_temp = 0; 
                    
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        // Actualiza de manera estática y sin parpadeos si estamos midiendo activamente
                        if (estado_pantalla == 4) {
                            sprintf(buffer_temp, "Temp:%0.2f C     ", temperatura); // Relleno de espacios al final
                            OLED_String(4, 0, buffer_temp); 
                        }
                    }
                    
                    // Solo envía reportes UART cuando la medición es válida (Estado 4)
                    if (estado_pantalla == 4) {
                        UART_SendString("--------------------------------------------\r\n");
                        UART_SendString("Temperatura : ");
                        UART_SendFloat(temperatura, 2);
                        UART_SendString(" C\r\n");
                        UART_SendString("Frec. Pulso : ");
                        UART_SendUInt(lpm);
                        UART_SendString(" LPM\r\n");
                        UART_SendString("--------------------------------------------\r\n\r\n");
                    }
                    DS18B20_StartConversion(); 
                }
                
                // =========================================================
                // MÁQUINA DE ESTADOS DE HARDWARE Y CONTROL DE PANTALLA
                // =========================================================
                if (datos_sensor.red < 15000) {
                    /* -----------------------------------------------------
                     * CONDICIÓN 1: SIN DEDO EN EL SENSOR
                     * ----------------------------------------------------- */
                    LED_FUNCIONAL = 0;
                    LED_ESPERA = 1;
                    LED_ALARMA = 0; 
                    BUZZER = 0;
                    
                    if (tiene_registro == 1) {
                        // NUEVO ESTADO (1): Mostrar historial/retención de datos previos
                        if (estado_pantalla != 1) {
                            OLED_Clear();
                            OLED_String(0, 0, "ULTIMA MEDICION ");
                            
                            sprintf(buffer_texto, "LPM: %-3u        ", ultimo_lpm);
                            OLED_String(2, 0, buffer_texto);
                            
                            sprintf(buffer_temp, "Temp:%0.2f C     ", ultima_temp);
                            OLED_String(4, 0, buffer_temp);
                            
                            OLED_String(6, 0, "Coloque el dedo ");
                            estado_pantalla = 1;
                        }
                    } else {
                        // ESTADO (0): Espera inicial limpia (nunca se ha tomado una muestra)
                        if (estado_pantalla != 0) {
                            OLED_Clear();
                            OLED_String(0, 0, "MONITOR CARDIACO");
                            OLED_String(3, 0, "Coloque el dedo ");
                            OLED_String(5, 0, "en el sensor    ");
                            estado_pantalla = 0;
                        }
                    }

                    lpm = 0;
                    last_lpm = 0;
                } 
                else {
                    /* -----------------------------------------------------
                     * CONDICIÓN 2: DEDO DETECTADO (PROCESANDO INFORMACIÓN)
                     * ----------------------------------------------------- */
                    LED_ESPERA = 0;
                    LED_FUNCIONAL = 1;
                    
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (temperatura < 32.0 || lpm == 0) {
                        /* ESTADO 2: PROCESANDO / CALCULANDO */
                        LED_ALARMA = 0;
                        BUZZER = 0;

                        if (estado_pantalla != 2) {
                            OLED_Clear();
                            OLED_String(0, 0, "PROCESANDO      ");
                            OLED_String(2, 0, "MEDICIONES...   ");
                            OLED_String(5, 0, "ESPERE          ");
                            estado_pantalla = 2;
                        }
                    }
                    else {
                        /* ESTADO 4: MEDICIÓN HUMANA COMPLETA Y ESTABLE */

                        // Evaluación de umbrales médicos para alertas de alarma
                        if (temperatura > TEMP_MAX || temperatura < TEMP_MIN || lpm > BPM_MAX || lpm < BPM_MIN) {
                            LED_ALARMA = 1;
                            BUZZER = 1;
                        } else {
                            LED_ALARMA = 0;
                            BUZZER = 0;
                        }

                        // OPTIMIZACIÓN DE PARPADEO:
                        // Si cambiamos de estado hacia el 4, limpiamos y dibujamos la estructura fija una sola vez.
                        if (estado_pantalla != 4) {
                            OLED_Clear();
                            OLED_String(0, 0, "PULSIOXIMETRO   ");
                            estado_pantalla = 4;
                            last_lpm = 999; // Forzamos la escritura inicial de los datos numéricos
                        }

                        // Actualización selectiva de variables en pantalla sin usar OLED_Clear()
                        if (latido_real || lpm != last_lpm) {
                            sprintf(buffer_texto, "LPM: %-3u        ", lpm); // Los espacios al final limpian residuos previos
                            OLED_String(2, 0, buffer_texto);

                            sprintf(buffer_temp, "Temp:%0.2f C     ", temperatura);
                            OLED_String(4, 0, buffer_temp);

                            last_lpm = lpm;

                            // Actualizar y asegurar el historial en las variables globales de retención
                            ultimo_lpm = lpm;
                            ultima_temp = temperatura;
                            tiene_registro = 1;
                        }
                    }
                }
            }
        }
    }
}