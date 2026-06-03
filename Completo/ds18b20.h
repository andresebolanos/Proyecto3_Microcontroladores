/**
 * @file    ds18b20.h
 * @brief   Librería para el sensor de temperatura DS18B20 (protocolo 1-Wire).
 * @details Implementa el protocolo 1-Wire bit a bit usando delays calibrados
 *          para un oscilador interno de 8 MHz en el PIC18F4550.
 *          Soporta lectura de temperatura en resolución de 12 bits (0.0625 °C).
 *
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 *
 * @note    Conexión hardware:
 *          - RD1 -> Bus Data del DS18B20 (con resistencia pull-up de 4.7kΩ a VDD)
 *          - VDD -> 3.3V o 5V
 *          - GND -> GND
 * @note    Esta librería está diseñada para un SOLO sensor en el bus (usa comando SKIP_ROM).
 * @warning Esta librería usa modo de alimentación normal (no parásita).
 *          La resistencia pull-up de 4.7kΩ es estrictamente obligatoria.
 * @warning Los tiempos están calibrados para FOSC = 8 MHz. Cambiar la frecuencia
 *          requiere reajustar todos los delays.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <xc.h>

/**
 * @defgroup DS18B20_Config Configuración del Sistema
 * @brief Constantes de configuración del oscilador y tiempos.
 * @{
 */

/** @brief Frecuencia del oscilador en Hz. Debe coincidir con OSCCON para __delay_ms. */
#ifndef _XTAL_FREQ
#define _XTAL_FREQ 8000000UL
#endif

/** @} */ // Fin de DS18B20_Config

/**
 * @defgroup DS18B20_Hardware Configuración del Pin 1-Wire
 * @brief Macros para el control del bus 1-Wire en el pin RD1.
 * @{
 */

/** @brief Puerto de lectura de datos donde está conectado el DS18B20. */
#define OW_PORT     PORTDbits.RD1

/** @brief Latch de salida del pin 1-Wire. */
#define OW_LAT      LATDbits.LATD1

/** @brief Registro de control de dirección del pin 1-Wire (1 = entrada, 0 = salida). */
#define OW_TRIS     TRISDbits.TRISD1

/** 
 * @brief Fuerza la línea a estado bajo (0V dominante).
 * @details Configura el pin como salida y escribe 0.
 */
#define OW_LOW()    do { OW_LAT = 0; OW_TRIS = 0; } while(0)

/** 
 * @brief Libera la línea (alta impedancia, pull-up sube a VDD).
 * @details Configura el pin como entrada. La resistencia pull-up eleva la línea.
 */
#define OW_RELEASE()  do { OW_TRIS = 1; } while(0)

/** @brief Lee el estado lógico actual de la línea 1-Wire. */
#define OW_READ()   (OW_PORT)

/** @} */ // Fin de DS18B20_Hardware

/**
 * @defgroup DS18B20_Timing Tiempos del Protocolo 1-Wire
 * @brief Constantes de tiempo para las operaciones del bus 1-Wire.
 * @note Basados en Maxim Application Note 126.
 * @{
 */

#define OW_RESET_LOW_US     500     /**< @brief Duración del pulso de reset (µs). Mínimo 480 µs. */
#define OW_RESET_RECOVER_US  70     /**< @brief Espera antes de leer presencia (µs). */
#define OW_RESET_WAIT_US    410     /**< @brief Espera restante tras leer presencia (µs). */

#define OW_WRITE_1_LOW_US     6     /**< @brief Tiempo en LOW para escribir '1' (µs). */
#define OW_WRITE_1_HIGH_US   64     /**< @brief Tiempo en HIGH para escribir '1' (µs). */
#define OW_WRITE_0_LOW_US    60     /**< @brief Tiempo en LOW para escribir '0' (µs). */
#define OW_WRITE_0_HIGH_US   10     /**< @brief Tiempo en HIGH para escribir '0' (µs). */

#define OW_READ_LOW_US        6     /**< @brief Tiempo en LOW para inicio de lectura (µs). */
#define OW_READ_SAMPLE_US     9     /**< @brief Espera antes de muestrear el bit (µs). */
#define OW_READ_RECOVER_US   55     /**< @brief Tiempo restante del slot de lectura (µs). */

/** @} */ // Fin de DS18B20_Timing

/**
 * @defgroup DS18B20_Commands Comandos del DS18B20
 * @brief Comandos ROM y funciones del sensor.
 * @{
 */

/** @brief Omite la selección de ROM (útil para un solo sensor en el bus). */
#define DS18B20_CMD_SKIP_ROM        0xCC

/** @brief Ordena iniciar una conversión de temperatura analógica a digital. */
#define DS18B20_CMD_CONVERT_T       0x44

/** @brief Ordena leer el contenido de la memoria scratchpad (9 bytes). */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

/** @} */ // Fin de DS18B20_Commands

/**
 * @defgroup DS18B20_Resolution Resolución y Tiempos del Sensor
 * @brief Configuración de resolución y tiempos de conversión.
 * @{
 */

/**
 * @brief Opciones del byte de configuración de resolución.
 * @details Bits [6:5] del Configuration Register:
 *          - 0x1F: 9  bits (0.5 °C,   ~93.75 ms)
 *          - 0x3F: 10 bits (0.25 °C,  ~187.5 ms)
 *          - 0x5F: 11 bits (0.125 °C, ~375 ms)
 *          - 0x7F: 12 bits (0.0625 °C,~750 ms) -> Por defecto de fábrica.
 */
#define DS18B20_RES_12BIT           0x7F

/** @brief Tiempo máximo de conversión a 12 bits en milisegundos. */
#define DS18B20_CONV_TIME_MS        750

/** @} */ // Fin de DS18B20_Resolution

/**
 * @defgroup DS18B20_ReturnCodes Códigos de Retorno
 * @brief Valores devueltos por las funciones del driver.
 * @{
 */

#define DS18B20_OK                  0   /**< @brief Éxito de la operación. */
#define DS18B20_ERR_NO_DEVICE       1   /**< @brief Error: Sin dispositivo (falta pulso de presencia). */

/** @} */ // Fin de DS18B20_ReturnCodes

/**
 * @defgroup DS18B20_Functions API Pública del Driver
 * @brief Funciones para inicializar, convertir y leer temperatura.
 * @{
 */

/** 
 * @brief Inicializa el pin del bus 1-Wire.
 * @details Configura el pin RD1 en alta impedancia (entrada). 
 *          La resistencia pull-up mantiene la línea en HIGH.
 */
void DS18B20_Init(void);

/** 
 * @brief Ejecuta la secuencia de Reset del protocolo 1-Wire.
 * @return DS18B20_OK si el sensor respondió con pulso de presencia,
 *         DS18B20_ERR_NO_DEVICE en caso contrario.
 */
unsigned char DS18B20_Reset(void);

/** 
 * @brief Inicia una conversión de temperatura en el sensor.
 * @details Envía los comandos SKIP_ROM y CONVERT_T.
 * @warning Función bloqueante: espera 750 ms a que termine la conversión a 12 bits.
 * @return DS18B20_OK si el comando se envió correctamente,
 *         DS18B20_ERR_NO_DEVICE si no hay sensor presente.
 */
unsigned char DS18B20_StartConversion(void);

/** 
 * @brief Lee la temperatura actual del sensor.
 * @param temperature Puntero donde se almacenará el valor en grados Celsius.
 * @details Lee los bytes LSB y MSB del scratchpad y los convierte usando la fórmula:
 *          temperatura = raw_signed * 0.0625f
 * @note No se implementa verificación CRC por simplicidad.
 *       Para aplicaciones críticas, se recomienda añadirla.
 * @return DS18B20_OK si la lectura fue exitosa,
 *         DS18B20_ERR_NO_DEVICE si no hay sensor presente.
 */
unsigned char DS18B20_ReadTemperature(float *temperature);

/** @} */ // Fin de DS18B20_Functions

#endif /* DS18B20_H */