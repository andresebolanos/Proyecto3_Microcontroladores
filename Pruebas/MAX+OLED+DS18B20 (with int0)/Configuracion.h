#ifndef CONFIGURACION_H
#define	CONFIGURACION_H

// CONFIG1H (Configuración del Oscilador)
#pragma config FOSC = INTOSCIO_EC // Oscilador Interno. Permite usar OSCCON = 0x72 para los 8MHz.

// CONFIG2L
#pragma config PWRT = ON      // Power-up Timer. Da un pequeño retraso al encender para estabilizar el voltaje.

// CONFIG2H (Perro Guardián)
#pragma config WDT = OFF      // Watchdog Timer..

// CONFIG3H (Configuración de Pines y Reset)
#pragma config PBADEN = OFF   // PORTB A/D Enable. ¡CRÍTICO PARA LA OLED! Inicia el puerto B como digital.
#pragma config MCLRE = ON     // Master Clear Enable. Pin de Reset activado (puedes ponerlo en OFF si no usas botón de reset).

// CONFIG4L (Programación y Compilador)
#pragma config LVP = OFF      // Low-Voltage Programming. Apagado para evitar problemas y liberar el pin RB5.
#pragma config XINST = OFF    // Extended Instruction Set. Debe estar apagado para que el compilador XC8 funcione bien.

#pragma config BOR = OFF      // Desactiva el Brown-out Reset para ahorrar batería

#endif	/* CONFIGURACION_H */