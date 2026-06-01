#ifndef OLED_LIBRERIA_H
#define	OLED_LIBRERIA_H

#include <xc.h>

/* Frecuencia del oscilador para los delays (si no está definida en otro lado) */
#ifndef _XTAL_FREQ
#define _XTAL_FREQ 8000000
#endif

/* =========================================
 * DEFINICIONES I2C Y OLED
 * ========================================= */
#define OLED_ADDR 0x78
#define OLED_CMD 0x00 
#define OLED_DATA 0x40 
#define I2C_BAUDRATE 19

/* =========================================
 * PROTOTIPOS DE FUNCIONES I2C
 * ========================================= */
void I2C_Init(void);
void I2C_Ready(void);
void I2C_Start(unsigned char addr);
void I2C_Write(unsigned char data);
void I2C_Stop(void);
void I2C_Restart(void);
unsigned char I2C_Read(unsigned char ack);

/* =========================================
 * PROTOTIPOS DE FUNCIONES OLED
 * ========================================= */
void OLED_Init(void);
void OLED_Comando(unsigned char cmd);
void OLED_Dato(unsigned char dato);
void OLED_Clear(void);
void OLED_SetCursor(unsigned char pagina, unsigned char col);
void OLED_Char(unsigned char c);
void OLED_String(unsigned char pagina, unsigned char col, const char *texto);

#endif	/* OLED_LIBRERIA_H */