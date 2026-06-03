/**
 * @file OLED_Libreria.h
 * @brief Cabecera de la librería para comunicación I2C y control de pantalla OLED SSD1306.
 * @details Define las direcciones, constantes y prototipos de funciones necesarios para
 * establecer la comunicación I2C por hardware en el PIC18F4550 y gestionar la interfaz
 * gráfica básica de una pantalla OLED monocromática de 128x64 o 128x32.
 * 
 * @author  Andres Bolaños
 * @date    2026
 * @version 1.0
 */

#ifndef OLED_LIBRERIA_H
#define	OLED_LIBRERIA_H

#include <xc.h>

/**
 * @defgroup OLED_Config Configuración del Hardware
 * @brief Constantes de configuración para el bus I2C y la pantalla OLED.
 * @{
 */

/**
 * @brief Frecuencia del oscilador principal para los delays internos de XC8.
 * @details Si el usuario ya ha definido _XTAL_FREQ en su configuración, este valor no se sobrescribe.
 * @note Este valor DEBE coincidir con la frecuencia real del oscilador del PIC.
 */
#ifndef _XTAL_FREQ
#define _XTAL_FREQ 8000000
#endif

/** 
 * @brief Dirección I2C de la pantalla OLED (0x3C desplazada 1 bit a la izquierda).
 * @note En el protocolo I2C, la dirección se envía en los 7 bits más significativos.
 *       El valor 0x78 corresponde a 0x3C << 1, ya preparado para ser enviado en el primer byte.
 */
#define OLED_ADDR 0x78

/** @brief Byte de control que indica al SSD1306 que el siguiente byte es un COMANDO. */
#define OLED_CMD  0x00

/** @brief Byte de control que indica al SSD1306 que el siguiente byte es un DATO (píxeles). */
#define OLED_DATA 0x40

/** 
 * @brief Valor de recarga para el registro SSPADD.
 * @details Configura la frecuencia del reloj I2C a aproximadamente 100kHz, asumiendo Fosc = 8MHz.
 * @see Hoja de datos del PIC18F4550, sección MSSP para la fórmula de cálculo.
 */
#define I2C_BAUDRATE 19

/** @} */ // Fin de OLED_Config

/**
 * @defgroup I2C_Driver Funciones de Bajo Nivel I2C (MSSP)
 * @brief Implementación del protocolo I2C maestro usando el módulo MSSP del PIC18F4550.
 * @{
 */

/** @brief Inicializa el módulo MSSP como maestro I2C a 100kHz. */
void I2C_Init(void);

/** 
 * @brief Espera a que el bus I2C esté listo para una nueva operación.
 * @details Verifica que el buffer no esté lleno (BF) y que no haya ninguna condición activa (SEN, PEN, etc.).
 */
void I2C_Ready(void);

/** 
 * @brief Genera una condición de START y envía la dirección del esclavo.
 * @param addr Dirección I2C del esclavo (7 bits desplazados a la izquierda, bit LSB = R/W).
 * @note La dirección debe incluir el bit de lectura/escritura en su LSB.
 */
void I2C_Start(unsigned char addr);

/** 
 * @brief Envía un byte de datos por el bus I2C.
 * @param data Byte a transmitir.
 */
void I2C_Write(unsigned char data);

/** @brief Genera una condición de STOP para liberar el bus I2C. */
void I2C_Stop(void);

/** 
 * @brief Genera una condición de RESTART (Reinicio) sin liberar el bus.
 * @details Útil para cambiar de escritura a lectura en la misma comunicación.
 */
void I2C_Restart(void);

/** 
 * @brief Lee un byte desde el bus I2C.
 * @param ack 1 = enviar ACK (reconocimiento), 0 = enviar NACK (fin de lectura).
 * @return El byte leído desde el esclavo.
 * @warning El parámetro ack sigue la convención: 1 para ACK (esperar más datos), 0 para NACK (último byte).
 */
unsigned char I2C_Read(unsigned char ack);

/** @} */ // Fin de I2C_Driver

/**
 * @defgroup OLED_Driver Control de Pantalla SSD1306
 * @brief Funciones de alto nivel para manejar la pantalla OLED.
 * @{
 */

/** @brief Inicializa la pantalla OLED enviando la secuencia completa de comandos. */
void OLED_Init(void);

/** 
 * @brief Envía un comando al controlador SSD1306.
 * @param cmd Byte de comando (ver hoja de datos del SSD1306).
 */
void OLED_Comando(unsigned char cmd);

/** 
 * @brief Envía un dato (píxeles) a la memoria GRAM de la OLED.
 * @param dato Byte con el patrón de píxeles (1 = encendido, 0 = apagado).
 */
void OLED_Dato(unsigned char dato);

/** @brief Limpia completamente la pantalla (todos los píxeles apagados). */
void OLED_Clear(void);

/** 
 * @brief Posiciona el cursor de escritura en la pantalla.
 * @param pagina Fila lógica (0 a 7, cada página son 8 píxeles de alto).
 * @param col Columna horizontal (0 a 127).
 */
void OLED_SetCursor(unsigned char pagina, unsigned char col);

/** 
 * @brief Dibuja un carácter ASCII en la posición actual del cursor.
 * @param c Carácter a dibujar (ASCII imprimible del 32 al 126).
 * @note Si el carácter está fuera del rango, se dibuja un espacio.
 */
void OLED_Char(unsigned char c);

/** 
 * @brief Escribe una cadena de texto en la pantalla OLED.
 * @param pagina Fila inicial (0 a 7).
 * @param col Columna inicial (0 a 127).
 * @param texto Puntero a la cadena (debe terminar en '\0').
 */
void OLED_String(unsigned char pagina, unsigned char col, const char *texto);

/** @} */ // Fin de OLED_Driver

#endif	/* OLED_LIBRERIA_H */