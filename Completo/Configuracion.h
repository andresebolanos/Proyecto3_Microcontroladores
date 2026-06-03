/**
 * @file    Configuracion.h
 * @brief   Configuración de los bits de configuración (Fuses) para el PIC18F4550.
 * @details Define los parámetros críticos de arranque del microcontrolador, incluyendo
 *          la selección del oscilador interno, deshabilitación de pines analógicos por defecto,
 *          y protección de hardware para asegurar la estabilidad eléctrica de los sensores.
 *
 * @author  Andres Bolanos
 * @date    2026
 * @version 1.0
 *
 * @note    Estos parámetros se graban en la memoria de configuración del PIC
 *          y son cargados antes que el propio firmware.
 */

#ifndef CONFIGURACION_H
#define CONFIGURACION_H

/**
 * @defgroup Config_Fuses Bits de Configuración (Fuses)
 * @brief Configuración hardware del PIC18F4550 grabada en memoria no volátil.
 * @{
 */

/**
 * @defgroup Config_Osc Configuración del Sistema de Reloj
 * @{
 */

/** 
 * @brief Oscilador Interno activo.
 * @details Bloquea pines RA6/RA7 para E/S digital estándar y permite 
 *          usar OSCCON = 0x72 para fijar 8 MHz.
 */
#pragma config FOSC = INTOSCIO_EC

/** @} */ // Fin de Config_Osc

/**
 * @defgroup Config_Power Seguridad Eléctrica y Temporizadores de Arranque
 * @{
 */

/** 
 * @brief Power-up Timer activo.
 * @details Retrasa el arranque del micro ~65ms al encender para permitir 
 *          que la tensión VDD se estabilice antes de ejecutar código.
 */
#pragma config PWRT = ON

/** 
 * @brief Brown-out Reset apagado.
 * @details Evita reinicios no deseados si hay caídas leves de voltaje 
 *          por picos de consumo de los LEDs, optimizando la duración de la batería.
 * @warning Deshabilitar BOR puede causar comportamiento errático si la 
 *          alimentación es inestable. Usar con precaución.
 */
#pragma config BOR = OFF

/** @} */ // Fin de Config_Power

/**
 * @defgroup Config_WDT Temporizador Perro Guardián (Watchdog)
 * @{
 */

/** 
 * @brief Watchdog Timer inactivo.
 * @details Evita que el sistema se reinicie automáticamente si el bucle 
 *          principal o el modo Sleep se prolongan más de lo esperado.
 */
#pragma config WDT = OFF

/** @} */ // Fin de Config_WDT

/**
 * @defgroup Config_Pins Configuración de Pines, Reset e Interfaz de Depuración
 * @{
 */

/** 
 * @brief PORTB A/D Enable APAGADO.
 * @details ¡CRÍTICO PARA I2C! Configura los pines del PORTB (como RB2, RB0, RB1) 
 *          como digitales de fábrica, permitiendo la comunicación I2C con la OLED.
 * @warning Si esta configuración no está en OFF, los pines RB0 y RB1 
 *          funcionarán como entradas analógicas, impidiendo la comunicación I2C.
 */
#pragma config PBADEN = OFF

/** 
 * @brief Master Clear activo.
 * @details Habilita el pin RE3/MCLR externo como pin de Reset por hardware.
 *          Permite reiniciar el sistema mediante un pulsador externo.
 */
#pragma config MCLRE = ON

/** @} */ // Fin de Config_Pins

/**
 * @defgroup Config_Programming Modo de Programación y Soporte del Compilador
 * @{
 */

/** 
 * @brief Low-Voltage Programming apagado.
 * @details Libera el pin RB5 para uso digital general y previene 
 *          grabaciones accidentales del firmware por ruido en la línea.
 */
#pragma config LVP = OFF

/** 
 * @brief Extended Instruction Set inactivo.
 * @details Requerido obligatoriamente para garantizar la compatibilidad 
 *          del compilador XC8 con la arquitectura base del PIC18F4550.
 */
#pragma config XINST = OFF

/** @} */ // Fin de Config_Programming

/** @} */ // Fin de Config_Fuses

#endif /* CONFIGURACION_H */