/**
 * @file    ds18b20.h
 * @brief   Librería para el sensor de temperatura DS18B20 (protocolo 1-Wire).
 * @details Implementa el protocolo 1-Wire bit a bit usando delays calibrados
 *          para un oscilador interno de 8 MHz en el PIC18F4550.
 *          Soporta lectura de temperatura en resolución de 12 bits (0.0625 °C).
 *
 * @author  Tu Nombre
 * @date    2026
 * @version 1.0
 *
 * @note    Conexión hardware:
 *          - RB0  ? Data del DS18B20 (con resistencia pull-up de 4.7 k? a VDD)
 *          - VDD  ? 3.3V o 5V
 *          - GND  ? GND
 *
 * @warning Esta librería usa modo de alimentación normal (no parásita).
 *          La resistencia pull-up de 4.7 k? es obligatoria.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <xc.h>

// =========================================================
// FRECUENCIA DEL OSCILADOR (requerida por __delay_ms)
// =========================================================
#ifndef _XTAL_FREQ
/** @brief Frecuencia del oscilador en Hz. Debe coincidir con OSCCON. */
#define _XTAL_FREQ 8000000UL
#endif

// =========================================================
// CONFIGURACIÓN DEL PIN 1-WIRE
// =========================================================

/** @brief Puerto donde está conectado el DS18B20 */
#define OW_PORT     PORTDbits.RD1

/** @brief Latch de salida del pin 1-Wire */
#define OW_LAT      LATDbits.LATD1

/** @brief Registro de dirección del pin 1-Wire */
#define OW_TRIS     TRISDbits.TRISD1

/** @brief Pone la línea en estado bajo (dominante) */
#define OW_LOW()    do { OW_LAT = 0; OW_TRIS = 0; } while(0)

/** @brief Libera la línea (la resistencia pull-up la lleva a HIGH) */
#define OW_RELEASE()  do { OW_TRIS = 1; } while(0)

/** @brief Lee el estado actual de la línea 1-Wire */
#define OW_READ()   (OW_PORT)

// =========================================================
// COMANDOS ROM DEL DS18B20
// =========================================================

/** @brief Omite la selección de ROM (un solo sensor en el bus) */
#define DS18B20_CMD_SKIP_ROM        0xCC

/** @brief Inicia una conversión de temperatura */
#define DS18B20_CMD_CONVERT_T       0x44

/** @brief Lee el contenido del scratchpad (9 bytes) */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

// =========================================================
// RESOLUCIÓN DEL SENSOR
// =========================================================

/**
 * @brief Byte de configuración de resolución del DS18B20.
 * @details Bits [5:6] del Configuration Register:
 *          - 0x1F ? 9  bits (0.5 °C,   ~93.75 ms)
 *          - 0x3F ? 10 bits (0.25 °C,  ~187.5 ms)
 *          - 0x5F ? 11 bits (0.125 °C, ~375 ms)
 *          - 0x7F ? 12 bits (0.0625 °C,~750 ms)  ? Por defecto
 */
#define DS18B20_RES_12BIT           0x7F

/** @brief Tiempo máximo de conversión a 12 bits en ms */
#define DS18B20_CONV_TIME_MS        750

// =========================================================
// CÓDIGOS DE RETORNO
// =========================================================

/** @brief Operación exitosa */
#define DS18B20_OK                  0

/** @brief Sin dispositivo detectado en el bus (sin pulso de presencia) */
#define DS18B20_ERR_NO_DEVICE       1

// =========================================================
// PROTOTIPOS DE FUNCIONES PÚBLICAS
// =========================================================

/**
 * @brief   Inicializa el pin 1-Wire y libera el bus.
 * @details Configura RB0 como entrada (pull-up externo mantiene HIGH).
 *          Debe llamarse una vez al inicio del programa.
 */
void DS18B20_Init(void);

/**
 * @brief   Envía el pulso de reset y detecta pulso de presencia.
 * @details El master baja la línea ?480 µs, luego la libera.
 *          El sensor responde con un pulso bajo de 60?240 µs.
 * @return  DS18B20_OK          si el sensor respondió correctamente.
 * @return  DS18B20_ERR_NO_DEVICE si no se detectó presencia.
 */
unsigned char DS18B20_Reset(void);

/**
 * @brief   Inicia una conversión de temperatura en el sensor.
 * @details Envía: SKIP ROM ? CONVERT T.
 *          Luego espera el tiempo de conversión (750 ms a 12 bits).
 * @return  DS18B20_OK           si el reset inicial tuvo éxito.
 * @return  DS18B20_ERR_NO_DEVICE si no se encontró el sensor.
 */
unsigned char DS18B20_StartConversion(void);

/**
 * @brief   Lee la temperatura del scratchpad del DS18B20.
 * @details Envía: SKIP ROM ? READ SCRATCHPAD, lee 2 bytes (Byte0 y Byte1),
 *          combina los bytes y convierte a grados Celsius.
 * @param   temperature  Puntero a float donde se almacenará el resultado en °C.
 * @return  DS18B20_OK            si la lectura fue exitosa.
 * @return  DS18B20_ERR_NO_DEVICE si no se encontró el sensor.
 */
unsigned char DS18B20_ReadTemperature(float *temperature);

#endif /* DS18B20_H */