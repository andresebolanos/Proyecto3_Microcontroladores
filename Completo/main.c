/**
 * @mainpage Monitor Portátil de Signos Vitales
 *
 * @section intro Introducción
 *
 * El presente proyecto consiste en el desarrollo de un monitor portátil de signos
 * vitales basado en el microcontrolador PIC18F4550. El sistema integra un sensor
 * MAX30102 para la medición de la frecuencia cardíaca y un sensor DS18B20 para la
 * medición de la temperatura corporal, permitiendo visualizar la información en
 * una pantalla OLED SSD1306 y transmitir los datos mediante comunicación UART.
 *
 * El firmware implementa una arquitectura modular basada en librerías independientes
 * para cada periférico y sensor, facilitando el mantenimiento, escalabilidad y
 * reutilización del código. Además, incorpora indicadores visuales y sonoros para
 * alertar al usuario cuando los parámetros medidos se encuentran fuera de los rangos
 * establecidos.
 *
 * @section modulos Módulos Principales
 *
 * - MAX30102_Libreria: Lectura y procesamiento de frecuencia cardíaca.
 * - ds18b20: Medición de temperatura corporal.
 * - OLED_Libreria: Interfaz gráfica en pantalla OLED SSD1306.
 * - uart: Comunicación serial para telemetría.
 * - main: Integración general del sistema y control de estados.
 *
 * @section autores Autores
 *
 * - Andrés Bolaños
 * - Jeison Tuquerrez
 *
 * @date 2026
 */

/**
 * @file main.c
 * @brief Aplicación principal del Monitor Portátil de Signos Vitales.
 *
 * Este módulo implementa la lógica principal del sistema encargado
 * del monitoreo de frecuencia cardíaca y temperatura corporal mediante
 * los sensores MAX30102 y DS18B20.
 *
 * Funcionalidades implementadas:
 * - Adquisición periódica de señales biomédicas.
 * - Visualización de datos mediante pantalla OLED SSD1306.
 * - Comunicación serial UART para monitoreo externo.
 * - Alarmas visuales y sonoras por valores fuera de rango.
 * - Retención de la última medición válida (Data Hold).
 * - Modo de bajo consumo mediante Sleep/Wakeup.
 * - Estabilización térmica del sensor DS18B20.
 *
 * @author Andres Bolaños
 * @author Jeison Tuquerres
 * @date 2026
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"     
#include <stdio.h>    
#include <xc.h>

/* =========================================================
 * 1. ASIGNACI�N COMPLETA DE PINES (PUERTO D y PUERTO B)
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

#define TRIS_BOTON_SLEEP     TRISBbits.TRISB2 // Bot�n INT2

#define TEMP_MAX 37.5
#define TEMP_MIN 32.0  // Umbral donde se considera temperatura humana v�lida
#define BPM_MAX 100
#define BPM_MIN 60

/**
 * @brief Bandera activada por Timer0.
 *
 * Indica al programa principal que debe realizar una nueva
 * iteración del proceso de muestreo.
 */
volatile unsigned char flag_nueva_muestra = 0;

/**
 * @brief Bandera de cambio de estado energético.
 *
 * Se activa cuando ocurre una interrupción externa INT2,
 * solicitando la transición entre los modos activo y Sleep.
 */
volatile unsigned char flag_cambiar_power = 0;

/**
 * @brief Rutina de servicio de interrupciones.
 *
 * Gestiona las interrupciones generadas por:
 * - Timer0: base de tiempo de 50 Hz para muestreo.
 * - INT2: control del modo Sleep/Wakeup mediante pulsador.
 *
 * La rutina únicamente actualiza banderas de control para
 * minimizar el tiempo de ejecución dentro de la ISR.
 */
void __interrupt() ISR(void) {
    // 1. Interrupci�n de Timer0 (Muestreo 50Hz)
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;  
        TMR0H = 0x63; 
        TMR0L = 0xC0; 
        flag_nueva_muestra = 1;  
    }
    
    // 2. Interrupci�n Externa INT2 (Bot�n en RB2 presionado)
    if (INTCON3bits.INT2IF) {
        INTCON3bits.INT2IF = 0;         // Limpiar bandera de interrupci�n INT2
        flag_cambiar_power = 1;         // Avisar al bucle principal
    }
}

/**
 * @brief Apaga los periféricos antes de ingresar al modo Sleep.
 *
 * Desactiva LEDs, buzzer, pantalla OLED y coloca el sensor
 * MAX30102 en modo de bajo consumo para minimizar el consumo
 * energético durante la suspensión del sistema.
 */
void Apagar_Sistema_Completo(void) {
    LATD = 0x00;         // Apagar todos los LEDs y Buzzer
    OLED_Clear();        // Limpiar pantalla 
    MAX30102_Shutdown(); // Apagar diodos internos del ox�metro
}

/**
 * @brief Función principal del sistema.
 *
 * Realiza la inicialización de periféricos y ejecuta la máquina
 * de estados principal encargada del monitoreo biomédico.
 *
 * Estados implementados:
 * - Estado 0: Espera inicial.
 * - Estado 1: Retención de la última medición.
 * - Estado 2: Procesamiento y estabilización de sensores.
 * - Estado 4: Visualización activa de mediciones.
 *
 * Funciones principales:
 * - Adquisición de datos del MAX30102.
 * - Lectura periódica del DS18B20.
 * - Cálculo de frecuencia cardíaca.
 * - Gestión de alarmas.
 * - Comunicación UART.
 * - Control de energía mediante Sleep/Wakeup.
 *
 * @note El DS18B20 utiliza un periodo de estabilización de
 * aproximadamente 10 segundos antes de habilitar la visualización
 * de la temperatura corporal.
 */
void main(void) {
    OSCCON = 0x72; // Configurar oscilador interno a 8MHz
    
    // Configurar pines de LEDs y Buzzer como salidas
    TRIS_LED_ENCENDIDO = 0;   TRIS_LED_PREPARANDO = 0;
    TRIS_LED_FUNCIONAL = 0;    TRIS_LED_ESPERA = 0;
    TRIS_LED_ALARMA = 0;       TRIS_BUZZER = 0;
    
    // Configurar RB2 (INT2) como entrada digital para el bot�n
    TRIS_BOTON_SLEEP = 1;
    INTCON2bits.INTEDG2 = 0;   // Interrupci�n por flanco de bajada para INT2 (Presionar a GND)
    INTCON3bits.INT2IF = 0;     // Limpiar bandera inicial de INT2
    INTCON3bits.INT2IE = 1;     // Habilitar la interrupci�n externa INT2
    
    // Estado inicial de arranque
    LED_ENCENDIDO = 1;   LED_PREPARANDO = 1;  
    LED_FUNCIONAL = 0;   LED_ESPERA = 0;   LED_ALARMA = 0;   BUZZER = 0;
    
    // Inicializaci�n de hardware
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
    
    /**
    * @brief Variables de retención de datos.
    *
    * Almacenan la última frecuencia cardíaca y temperatura
    * válidas obtenidas por el sistema.
    *
    * Estas variables permiten mostrar la última medición
    * cuando el usuario retira el dedo del sensor.
    */
    unsigned char ultimo_lpm = 0;
    float ultima_temp = 0.0;

    unsigned char temperatura_valida = 0;
    unsigned char tiene_registro = 0; 
    unsigned char sistema_dormido = 0; 

    /**
    * @brief Variables de estabilización térmica.
    *
    * El sensor DS18B20 requiere un tiempo de adaptación
    * para alcanzar equilibrio térmico con la piel del usuario.
    *
    * temperatura_estable:
    * Indica que el período de estabilización ha finalizado.
    *
    * ciclos_estabilizacion:
    * Contador utilizado para estimar aproximadamente
    * 10 segundos de estabilización térmica.
    */
    unsigned char temperatura_estable = 0;
    unsigned char ciclos_estabilizacion = 0;
    
    DS18B20_StartConversion(); 
    
    // CONFIGURACI�N TIMER0
    T0CON = 0x88; 
    TMR0H = 0x63; 
    TMR0L = 0xC0; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1; 
    INTCONbits.PEIE = 1;   
    INTCONbits.GIE = 1;    

    while(1) {
        
        /* GESTI�N DE ENERG�A: PETICI�N DE DORMIR O DESPERTAR */
        if (flag_cambiar_power == 1) {
            flag_cambiar_power = 0;
            __delay_ms(200);
            
            if (sistema_dormido == 0) {
                sistema_dormido = 1;
                Apagar_Sistema_Completo();
                
                SLEEP(); // El micro se duerme aqu�
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

        /* BUCLE DE MUESTREO NORMAL Y M�QUINA DE ESTADOS */
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                // BLOQUE LENTO (CADA 2 SEGUNDOS) - LECTURA DS18B20 + UART
                contador_muestras_temp++;
                if (contador_muestras_temp >= 100) {  
                    contador_muestras_temp = 0; 
                    
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {

                        temperatura_valida = 1;

                        /**
                        * @brief Proceso de estabilización térmica.
                        *
                        * Se requieren aproximadamente cinco conversiones válidas
                        * del DS18B20 antes de considerar estable la temperatura.
                        *
                        * Dado que cada lectura ocurre aproximadamente cada
                        * dos segundos, el período total de estabilización es
                        * cercano a los diez segundos.
                        */
                        if (ciclos_estabilizacion < 5)
                        {
                            ciclos_estabilizacion++;
                        }
                        else
                        {
                            temperatura_estable = 1;
                        }

                        /* Actualizaci�n normal cuando ya estamos mostrando resultados */
                        if (estado_pantalla == 4) {
                            sprintf(buffer_temp, "Temp:%0.2f C     ", temperatura);
                            OLED_String(4, 0, buffer_temp);
                        }
                    }
                    
                    // Solo env�a reportes UART cuando la medici�n es v�lida (Estado 4)
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
                
                /**
                * @brief Máquina de estados de interfaz y monitoreo.
                *
                * Estado 0:
                * Espera de usuario sin mediciones previas.
                *
                * Estado 1:
                * Visualización de la última medición almacenada.
                *
                * Estado 2:
                * Proceso de adquisición y estabilización de sensores.
                *
                * Estado 4:
                * Visualización activa de frecuencia cardíaca,
                * temperatura corporal y alarmas.
                */
                if (datos_sensor.red < 15000) {
                    /* -----------------------------------------------------
                     * CONDICI�N 1: SIN DEDO EN EL SENSOR
                     * ----------------------------------------------------- */
                    
                    temperatura_valida = 0;
                    temperatura_estable = 0;
                    ciclos_estabilizacion = 0;

                    LED_FUNCIONAL = 0;
                    LED_ESPERA = 1;
                    LED_ALARMA = 0; 
                    BUZZER = 0;
                    
                    if (tiene_registro == 1) {
                        // NUEVO ESTADO (1): Mostrar historial/retenci�n de datos previos
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
                     * CONDICI�N 2: DEDO DETECTADO (PROCESANDO INFORMACI�N)
                     * ----------------------------------------------------- */
                    LED_ESPERA = 0;
                    LED_FUNCIONAL = 1;
                    
                    unsigned char latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (!temperatura_estable || lpm == 0) {
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
                        /* ESTADO 4: MEDICI�N HUMANA COMPLETA Y ESTABLE */

                        // Evaluaci�n de umbrales m�dicos para alertas de alarma
                        if (temperatura > TEMP_MAX || temperatura < TEMP_MIN || lpm > BPM_MAX || lpm < BPM_MIN) {
                            LED_ALARMA = 1;
                            BUZZER = 1;
                        } else {
                            LED_ALARMA = 0;
                            BUZZER = 0;
                        }

                        // OPTIMIZACI�N DE PARPADEO:
                        // Si cambiamos de estado hacia el 4, limpiamos y dibujamos la estructura fija una sola vez.
                        if (estado_pantalla != 4) {
                            OLED_Clear();
                            OLED_String(0, 0, "PULSIOXIMETRO   ");
                            estado_pantalla = 4;
                            last_lpm = 999; // Forzamos la escritura inicial de los datos num�ricos
                        }

                        // Actualizaci�n selectiva de variables en pantalla sin usar OLED_Clear()
                        if (latido_real || lpm != last_lpm) {
                            sprintf(buffer_texto, "LPM: %-3u        ", lpm); // Los espacios al final limpian residuos previos
                            OLED_String(2, 0, buffer_texto);

                            sprintf(buffer_temp, "Temp:%0.2f C     ", temperatura);
                            OLED_String(4, 0, buffer_temp);

                            last_lpm = lpm;

                            // Actualizar y asegurar el historial en las variables globales de retenci�n
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