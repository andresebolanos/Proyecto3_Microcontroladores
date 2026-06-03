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
 * @file    main.c
 * @brief   Orquestador e Integración Final del Monitor Portátil de Signos Vitales.
 * @details Gestiona el bucle principal de ejecución del PIC18F4550. Integra
 *          el control de temporización por Timer0 a 50Hz, capturas asíncronas por 
 *          interrupción externa del botón de ahorro de energía (Sleep), procesamiento
 *          en tiempo real de temperatura médica y ritmo cardíaco, y alarmas audiovisuales.
 *
 * @author  Andres Bolanos, Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 */

#include "Configuracion.h"
#include "OLED_Libreria.h"
#include "MAX30102_Libreria.h"
#include "ds18b20.h"  
#include "uart.h"     

/**
 * @brief Se incluye stdio.h únicamente para sprintf().
 * @note sprintf() consume aproximadamente 2KB de ROM. Si se requiere
 *       optimizar memoria, reemplazar por conversiones manuales.
 */
#include <stdio.h>    

/* =========================================
 * DEFINICIONES DE HARDWARE (PINOUT)
 * ========================================= */

/**
 * @defgroup Pinout_Mapping Mapeo de Pines del Hardware
 * @brief Asignación de funciones a pines específicos del PIC18F4550.
 * @{
 */

/* --- LEDs de Estado --- */
#define LED_ENCENDIDO        LATDbits.LATD0  /**< @brief RD0: Sistema encendido/activo */
#define LED_PREPARANDO       LATDbits.LATD2  /**< @brief RD2: Inicialización en progreso */
#define LED_FUNCIONAL        LATDbits.LATD3  /**< @brief RD3: Dedo detectado correctamente */
#define LED_ESPERA           LATDbits.LATD4  /**< @brief RD4: Esperando colocación del dedo */
#define LED_ALARMA           LATDbits.LATD5  /**< @brief RD5: Alerta visual (parámetros fuera de rango) */
#define BUZZER               LATDbits.LATD6  /**< @brief RD6: Alerta sonora (zumbador piezoeléctrico) */

/* --- Registros de Dirección de los LEDs --- */
#define TRIS_LED_ENCENDIDO   TRISDbits.TRISD0
#define TRIS_LED_PREPARANDO  TRISDbits.TRISD2
#define TRIS_LED_FUNCIONAL   TRISDbits.TRISD3
#define TRIS_LED_ESPERA      TRISDbits.TRISD4
#define TRIS_LED_ALARMA      TRISDbits.TRISD5
#define TRIS_BUZZER          TRISDbits.TRISD6

/* --- Entradas --- */
#define TRIS_BOTON_SLEEP     TRISBbits.TRISB2 /**< @brief RB2: Botón de sleep (INT2) */

/** @} */ // Fin de Pinout_Mapping

/* =========================================
 * CONSTANTES DEL SISTEMA
 * ========================================= */

/**
 * @defgroup System_Constants Constantes del Sistema
 * @brief Parámetros de configuración del comportamiento del firmware.
 * @{
 */

/** 
 * @defgroup Clinical_Thresholds Umbrales de Seguridad Clínica
 * @brief Valores límite para activar el protocolo de alarma médica.
 * @{
 */
#define TEMP_MAX 37.5f   /**< @brief Temperatura máxima normal (°C) - Por encima: fiebre */
#define TEMP_MIN 30.0f   /**< @brief Temperatura mínima normal (°C) - Por debajo: hipotermia/error */
#define BPM_MAX  100     /**< @brief Frecuencia cardíaca máxima normal (LPM) - Taquicardia */
#define BPM_MIN  60      /**< @brief Frecuencia cardíaca mínima normal (LPM) - Bradicardia */
/** @} */

/**
 * @defgroup Timing_Constants Constantes de Temporización
 * @brief Valores para temporizadores y retardos.
 * @{
 */
#define TIMER0_PRELOAD_H   0x63  /**< @byte Alto para precarga de Timer0 (20ms @ 8MHz) */
#define TIMER0_PRELOAD_L   0xC0  /**< @byte Bajo para precarga de Timer0 (20ms @ 8MHz) */
#define SAMPLES_PER_UPDATE 100   /**< @brief Muestras necesarias para actualizar temperatura (2 seg @ 50Hz) */
#define DEBOUNCE_DELAY_MS  200   /**< @brief Retardo antirrebote para el botón de sleep (ms) */
#define MIN_VALID_TEMP     20.0f /**< @brief Temperatura mínima para considerar válida y guardar en histórico */

/** 
 * @brief Umbral de detección de dedo en el sensor MAX30102.
 * @note Debe coincidir con MAX_MIN_SIGNAL definido en MAX30102_Libreria.h
 */
#define DETECTION_THRESHOLD 15000

/** @} */

/**
 * @defgroup Display_States Estados de la Pantalla OLED
 * @brief Valores para la máquina de estados de la interfaz gráfica.
 * @{
 */
#define DISPLAY_STATE_IDLE       0  /**< @brief Pantalla de espera (sin dedo) */
#define DISPLAY_STATE_CALCULATING 1  /**< @brief Calculando pulso (dedo detectado, estabilizando) */
#define DISPLAY_STATE_MEASURING   2  /**< @brief Mostrando mediciones activas */
#define DISPLAY_STATE_FORCE_REFRESH 99 /**< @brief Forzar refresco completo de pantalla */
/** @} */

/** @} */ // Fin de System_Constants

/* =========================================
 * VARIABLES GLOBALES (INTERRUPCIÓN)
 * ========================================= */

/**
 * @defgroup Interrupt_Flags Banderas de Interrupción
 * @brief Variables modificadas exclusivamente dentro del contexto de la ISR.
 * @note Todas son volatile para evitar optimizaciones del compilador.
 * @{
 */

/**
 * @brief Bandera activada a 50Hz por Timer0.
 * @details Indica al bucle principal que existe una nueva muestra disponible.
 */
volatile unsigned char flag_nueva_muestra = 0;

/** 
* @brief Bandera activada por INT2 (botón). Solicita cambio de estado sleep/wake. 
*/
volatile unsigned char flag_cambiar_power = 0;

/** @} */ // Fin de Interrupt_Flags

/* =========================================
 * PROTOTIPOS DE FUNCIONES PRIVADAS
 * ========================================= */

/**
 * @brief   Desactiva los actuadores y pone en bajo consumo los periféricos esclavos.
 * @details Apaga todos los transistores/LEDs conectados al puerto D, limpia la pantalla
 *          OLED para que sus píxeles no consuman energía y envía el comando físico 
 *          de SHUTDOWN al integrado MAX30102 apagando sus LEDs.
 * @post    El sistema está listo para entrar en modo SLEEP.
 */
static void Apagar_Sistema_Completo(void);

/* =========================================
 * IMPLEMENTACIÓN DE FUNCIONES PRIVADAS
 * ========================================= */

/**
 * @brief   Desactiva los actuadores y pone en bajo consumo los periféricos esclavos.
 * @details Apaga todos los transistores/LEDs conectados al puerto D, limpia la pantalla
 *          OLED para que sus píxeles no consuman energía y envía el comando físico 
 *          de SHUTDOWN al integrado MAX30102 apagando sus matrices de luz LED roja.
 */
static void Apagar_Sistema_Completo(void) {
    LATD = 0x00;         /* Apaga todos los LEDs y el buzzer */
    OLED_Clear();        /* Limpia la pantalla (píxeles apagados = menor consumo) */
    MAX30102_Shutdown(); /* Apaga el LED rojo del sensor */
}

/* =========================================
 * RUTINA DE SERVICIO DE INTERRUPCIÓN
 * ========================================= */

/**
 * @brief   Rutina de Servicio de Interrupción Única (ISR).
 * @details Administra los vectores de prioridad del PIC18F4550 de forma unificada:
 *          
 *          1. **Timer0 (Muestreo a 50Hz)**:
 *             - Limpia la bandera TMR0IF
 *             - Recarga los valores de precarga de 16 bits
 *             - Activa flag_nueva_muestra para el lazo principal
 *          
 *          2. **INT2 (Botón de Sleep en RB2)**:
 *             - Limpia INT2IF
 *             - Activa flag_cambiar_power para solicitar cambio de estado
 * 
 * @warning Esta ISR no debe contener retardos ni funciones complejas.
 *          Solo maneja banderas y recargas de timer.
 */
void __interrupt() ISR(void) {
    /* ===== 1. Interrupción de Timer0 (Muestreo Estricto a 50Hz) ===== */
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;          /* Limpia bandera */
        TMR0H = TIMER0_PRELOAD_H;       /* Recarga byte alto */
        TMR0L = TIMER0_PRELOAD_L;       /* Recarga byte bajo */
        flag_nueva_muestra = 1;         /* Notifica al bucle principal */
    }
    
    /* ===== 2. Interrupción Externa INT2 (Botón de Ahorro en RB2) ===== */
    if (INTCON3bits.INT2IF) {
        INTCON3bits.INT2IF = 0;         /* Limpia bandera */
        flag_cambiar_power = 1;         /* Solicita cambio de estado de energía */
    }
}

/* =========================================
 * FUNCIÓN PRINCIPAL
 * ========================================= */

/**
 * @brief   Punto de entrada principal del firmware del monitor médico.
 * @details Realiza la configuración completa del sistema:
 *          
 *          **1. Configuración inicial:**
 *          - Oscilador interno a 8 MHz (OSCCON)
 *          - Direcciones E/S digitales (TRIS)
 *          - LEDs y periféricos de salida
 *          
 *          **2. Inicialización de drivers:**
 *          - UART (telemetría)
 *          - I2C (comunicación con OLED y MAX30102)
 *          - OLED (pantalla)
 *          - MAX30102 (sensor de pulso)
 *          - DS18B20 (sensor de temperatura)
 *          
 *          **3. Configuración de interrupciones:**
 *          - Timer0 para base de tiempo de 50Hz
 *          - INT2 para botón de sleep
 *          - Interrupciones globales habilitadas
 *          
 *          **4. Bucle principal:**
 *          - Gestión de sleep/wake por botón
 *          - Muestreo a 50Hz sincronizado por Timer0
 *          - Procesamiento de señales médicas
 *          - Actualización de pantalla y alarmas
 * 
 * @post    El sistema opera indefinidamente en el bucle principal.
 */
void main(void) {
    /* =========================================
     * 1. CONFIGURACIÓN DEL OSCILADOR
     * ========================================= */
    OSCCON = 0x72;      /* Oscilador interno a 8 MHz */
    
    /* =========================================
     * 2. CONFIGURACIÓN DE PINES DE SALIDA
     * ========================================= */
    TRIS_LED_ENCENDIDO = 0;   TRIS_LED_PREPARANDO = 0;
    TRIS_LED_FUNCIONAL = 0;    TRIS_LED_ESPERA = 0;
    TRIS_LED_ALARMA = 0;       TRIS_BUZZER = 0;
    
    /* =========================================
     * 3. CONFIGURACIÓN DEL BOTÓN DE SLEEP (INT2)
     * ========================================= */
    TRIS_BOTON_SLEEP = 1;                       /* Entrada digital */
    INTCON2bits.INTEDG2 = 0;                    /* Flanco de bajada (presión a GND) */
    INTCON3bits.INT2IF = 0;                     /* Limpia bandera */
    INTCON3bits.INT2IE = 1;                     /* Habilita interrupción INT2 */
    
    /* =========================================
     * 4. ESTADO VISUAL DE ARRANQUE
     * ========================================= */
    LED_ENCENDIDO = 1;   LED_PREPARANDO = 1;  
    LED_FUNCIONAL = 0;   LED_ESPERA = 0;   
    LED_ALARMA = 0;      BUZZER = 0;
    
    /* =========================================
     * 5. INICIALIZACIÓN DE DRIVERS Y PERIFÉRICOS
     * ========================================= */
    UART_Init();        /* Puerto serie para telemetría */
    I2C_Init();         /* Bus I2C para OLED y MAX30102 */
    OLED_Init();        /* Pantalla gráfica */
    MAX30102_Init();    /* Sensor de pulso */
    DS18B20_Init();     /* Sensor de temperatura 1-Wire */
    
    __delay_ms(500);          /* Estabilización */
    LED_PREPARANDO = 0;       /* Sistema listo */
    
    /* =========================================
     * 6. INTERFAZ DE USUARIO INICIAL
     * ========================================= */
    OLED_String(0, 0, "   PULSIOXIMETRO ");
    OLED_String(2, 0, " Coloque el dedo ");
    OLED_String(4, 0, " Temp: --.-- C   ");
    
    /* =========================================
     * 7. VARIABLES LOCALES
     * ========================================= */
     MAX30102_Sample datos_sensor;          /**< Muestra cruda del sensor MAX30102 */

    unsigned char lpm = 0;                 /**< Latidos por minuto actuales */
    unsigned char last_lpm = 0;            /**< Último LPM mostrado */
    unsigned char estado_pantalla = DISPLAY_STATE_IDLE;     /**< Estado actual de la interfaz OLED */

    float temperatura = 0.0f;             /**< Temperatura actual en °C */
    unsigned int contador_muestras_temp = 0; /**< Contador para actualización periódica de temperatura */

    char buffer_texto[16];                 /**< Buffer de texto para mostrar BPM */
    char buffer_temp[16];                  /**< Buffer de texto para mostrar temperatura */

    unsigned char ultimo_lpm = 0;          /**< Último BPM válido almacenado */
    float ultima_temp = 0.0f;              /**< Última temperatura válida almacenada */
    unsigned char tiene_registro = 0;      /**< Indica si existe histórico de mediciones */
    unsigned char sistema_dormido = 0;     /**< Estado actual del modo Sleep */

    unsigned char latido_real;             /**< Indica detección de latido válido */
    
    /* Inicia primera conversión del DS18B20 (no bloqueante) */
    DS18B20_StartConversion();
    
    /* =========================================
     * 8. CONFIGURACIÓN DEL TIMER0 (Base de tiempo 50Hz)
     * ========================================= */
    T0CON = 0x88;       /* Timer0 activo, 16 bits, sin prescaler (1 ciclo = 0.5µs @ 8MHz) */
    TMR0H = TIMER0_PRELOAD_H;
    TMR0L = TIMER0_PRELOAD_L;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;      /* Habilita interrupción de Timer0 */
    INTCONbits.PEIE = 1;        /* Habilita interrupciones periféricas */
    INTCONbits.GIE = 1;         /* Habilita interrupciones globales */

    /* =========================================
     * 9. BUCLE PRINCIPAL INFINITO
     * ========================================= */
    while(1) {
        
        /* ===== GESTIÓN DE ENERGÍA: SLEEP/WAKE ===== */
        if (flag_cambiar_power == 1) {
            flag_cambiar_power = 0;
            __delay_ms(DEBOUNCE_DELAY_MS);      /* Filtro antirrebote */
            
            if (sistema_dormido == 0) {
                /* --- ENTRAR EN MODO SLEEP --- */
                sistema_dormido = 1;
                Apagar_Sistema_Completo();
                
                SLEEP();    /* Detiene CPU - consumo ultra bajo */
                NOP();      /* Post-sleep: instrucción requerida por arquitectura */
            } else {
                /* --- DESPERTAR DEL SLEEP --- */
                sistema_dormido = 0;
                
                /* Re-inicialización de periféricos después del sleep */
                OSCCON = 0x72;          /* Restaurar frecuencia */
                I2C_Init();
                OLED_Init();
                MAX30102_Init();
                
                LED_ENCENDIDO = 1;
                estado_pantalla = DISPLAY_STATE_FORCE_REFRESH;
                flag_nueva_muestra = 0;
                DS18B20_StartConversion();
            }
        }

        /* Si el sistema está dormido, saltar todo el procesamiento */
        if (sistema_dormido == 1) {
            continue; 
        }

        /* ===== BUCLE DE MUESTREO SINCRONIZADO A 50Hz ===== */
        if (flag_nueva_muestra == 1) {
            flag_nueva_muestra = 0; 
            
            /* Lectura del sensor MAX30102 */
            if (MAX30102_ReadSample(&datos_sensor)) {
                
                /* --- ACTUALIZACIÓN DE TEMPERATURA (cada 2 segundos) --- */
                contador_muestras_temp++;
                if (contador_muestras_temp >= SAMPLES_PER_UPDATE) {  
                    contador_muestras_temp = 0; 
                    
                    /* Lectura del DS18B20 */
                    if (DS18B20_ReadTemperature(&temperatura) == DS18B20_OK) {
                        /* Actualizar pantalla si no estamos mostrando histórico */
                        if (!(estado_pantalla == DISPLAY_STATE_IDLE && tiene_registro == 1)) {
                            sprintf(buffer_temp, " Temp: %0.2f C   ", temperatura);
                            OLED_String(4, 0, buffer_temp); 
                        }
                    }
                    
                    /* Telemetría UART (solo en modo medición activa) */
                    if (estado_pantalla == DISPLAY_STATE_MEASURING) {
                        UART_SendString("--------------------------------------------\r\n");
                        UART_SendString("Temperatura : ");
                        UART_SendFloat(temperatura, 2);
                        UART_SendString(" C\r\n");
                        UART_SendString("Frec. Pulso : ");
                        UART_SendUInt(lpm);
                        UART_SendString(" LPM\r\n");
                        UART_SendString("--------------------------------------------\r\n\r\n");
                    }
                    
                    /* Iniciar nueva conversión para el próximo ciclo */
                    DS18B20_StartConversion();
                }
                
                /* ===== MÁQUINA DE ESTADOS DE HARDWARE Y PANTALLA ===== */
                
                /* ESTADO 0: SIN DEDO (Detección por umbral) */
                if (datos_sensor.red < DETECTION_THRESHOLD) {
                    LED_FUNCIONAL = 0;
                    LED_ESPERA = 1;
                    LED_ALARMA = 0; 
                    BUZZER = 0;
                    
                    /* Actualizar pantalla solo si cambió el estado */
                    if (estado_pantalla != DISPLAY_STATE_IDLE) {
                        OLED_Clear();
                        estado_pantalla = DISPLAY_STATE_IDLE;
                        
                        if (tiene_registro == 1) {
                            /* Data-Hold: Mostrar últimos valores registrados */
                            OLED_String(0, 0, " COLOQUE EL DEDO ");
                            sprintf(buffer_texto, " LPM Ult: %-3u   ", ultimo_lpm);
                            OLED_String(2, 0, buffer_texto);
                            sprintf(buffer_temp, " Tmp Ult: %0.2f C", ultima_temp);
                            OLED_String(4, 0, buffer_temp);
                        } else {
                            /* Primera vez: sin histórico */
                            OLED_String(0, 0, "   PULSIOXIMETRO ");
                            OLED_String(2, 0, " Coloque el dedo ");
                            OLED_String(4, 0, " Temp: --.-- C   ");
                        }
                    }
                    lpm = 0; 
                    last_lpm = 0;
                } 
                else {
                    /* ESTADO CON DEDO DETECTADO */
                    LED_ESPERA = 0;
                    LED_FUNCIONAL = 1;
                    
                    latido_real = MAX30102_ProcesarBPM(datos_sensor.red, &lpm);
                    
                    if (lpm > 0) {
                        /* --- MEDICIÓN ESTABLE --- */
                        
                        /* Verificación de alarmas clínicas */
                        if (temperatura > TEMP_MAX || temperatura < TEMP_MIN || 
                            lpm > BPM_MAX || lpm < BPM_MIN) {
                            LED_ALARMA = 1;  
                            BUZZER = 1;      /* Alarma activa */
                        } else {
                            LED_ALARMA = 0;  
                            BUZZER = 0;      /* Parámetros dentro del rango normal */
                        }
                        
                        /* Actualizar pantalla si hay cambios significativos */
                        if (estado_pantalla != DISPLAY_STATE_MEASURING || 
                            latido_real || lpm != last_lpm) {
                            OLED_String(0, 0, "   PULSIOXIMETRO ");
                            sprintf(buffer_texto, " LPM: %-3u        ", lpm);
                            OLED_String(2, 0, buffer_texto);
                            estado_pantalla = DISPLAY_STATE_MEASURING;
                            last_lpm = lpm;
                            
                            /* Guardar en histórico para data-hold */
                            ultimo_lpm = lpm;
                            if (temperatura > MIN_VALID_TEMP) {
                                ultima_temp = temperatura;
                            }
                            tiene_registro = 1;
                        }
                    } else {
                        /* --- ESTADO TRANSITORIO: Dedo detectado, estabilizando señal --- */
                        if (estado_pantalla != DISPLAY_STATE_CALCULATING) {
                            OLED_String(0, 0, "   PULSIOXIMETRO ");
                            OLED_String(2, 0, " Calculando...   ");
                            estado_pantalla = DISPLAY_STATE_CALCULATING;
                        }
                        LED_ALARMA = 0;  
                        BUZZER = 0;      /* Sin alarmas durante estabilización */
                    }
                }
            }
        }
    }
}