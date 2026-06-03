/**
 * @file OLED_Libreria.c
 * @brief Implementación de las funciones del bus I2C y el controlador OLED.
 * @details Contiene el mapa de caracteres ASCII 5x8, la lógica de bajo nivel del módulo MSSP
 * para la comunicación I2C, y la inicialización y manipulación de registros del chip SSD1306.
 * 
 * @author  Andres Bolaños
 * @date    2026
 * @version 1.0
 */

#include "OLED_Libreria.h"

/**
 * @defgroup Fuente_Fuente5x8 Fuente de Caracteres 5x8
 * @brief Matriz que contiene el mapa de bits de los caracteres ASCII imprimibles.
 * @{
 */

/**
 * @brief Matriz constante con la fuente de caracteres de 5x8 píxeles.
 * @details Mapea los caracteres ASCII imprimibles desde el 32 (espacio) hasta el 126 (~).
 *          Cada carácter se representa mediante 5 bytes.
 *          - Cada byte corresponde a una **columna** vertical del carácter.
 *          - El bit 0 de cada byte representa el píxel superior de esa columna.
 *          - El bit 7 representa el píxel inferior (aunque normalmente solo se usan 8 filas).
 * @note El índice en la matriz se calcula como (c - 32), donde 'c' es el código ASCII.
 */
const unsigned char font5x8[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32 (espacio) */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 42 * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46 . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 60 < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 = */
    {0x41,0x22,0x14,0x08,0x00}, /* 62 > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70 F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 77 M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 87 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 X */
    {0x07,0x08,0x70,0x08,0x07}, /* 89 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91 [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93 ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 f */
    {0x08,0x14,0x54,0x54,0x3C}, /* 103 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107 k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 } */
    {0x10,0x08,0x08,0x10,0x08}  /* 126 ~ */
};

/** @} */ // Fin de Fuente_Fuente5x8

/* =========================================
 * FUNCIONES I2C
 * ========================================= */

/**
 * @defgroup I2C_Impl Implementación de Funciones I2C
 * @brief Código de bajo nivel para el manejo del bus I2C.
 * @{
 */

/**
 * @brief Función de bloqueo que espera a que el bus I2C esté libre.
 * @details Verifica el bit BF (Buffer Full) y los bits de estado del bus en SSPCON2.
 * @note La máscara 0x1F verifica los bits RCEN, PEN, RSEN, SEN y ACKEN del registro SSPCON2.
 */
void I2C_Ready(void){
    while(SSPSTATbits.BF || (SSPCON2 & 0x1F));
    __delay_us(5);
}

/**
 * @brief Genera la condición de START en el bus I2C y envía la dirección.
 * @param addr Dirección I2C del dispositivo esclavo (incluye el bit de R/W en el LSB).
 * @see I2C_Stop, I2C_Restart
 */
void I2C_Start(unsigned char addr){
    I2C_Ready();
    SSPCON2bits.SEN = 1; 
    while(SSPCON2bits.SEN);
    SSPBUF = addr; 
    while(!PIR1bits.SSPIF);
    PIR1bits.SSPIF = 0;
}

/**
 * @brief Envía un byte de datos a través del bus I2C.
 * @param data El byte que se desea transmitir.
 */
void I2C_Write(unsigned char data){
    I2C_Ready();
    SSPBUF = data;
    while(!PIR1bits.SSPIF); 
    PIR1bits.SSPIF = 0;
}

/**
 * @brief Genera la condición de STOP en el bus I2C para liberar la línea.
 */
void I2C_Stop(void){
    I2C_Ready();
    SSPCON2bits.PEN = 1;
    while(SSPCON2bits.PEN); 
    PIR1bits.SSPIF = 0;
}

/**
 * @brief Inicializa el hardware MSSP del PIC18F4550 para funcionar como maestro I2C.
 * @details Configura RB0 y RB1 como entradas digitales (necesario para la arquitectura de colector abierto)
 * y establece los registros SSP para operación a 100kHz.
 */
void I2C_Init(void){
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB1 = 1;
    SSPSTAT = 0x80;
    SSPCON1 = 0x28;
    SSPCON2 = 0x00;
    SSPADD = I2C_BAUDRATE; 
    PIR1bits.SSPIF = 0;
}

/**
 * @brief Realiza la lectura de un byte desde un esclavo I2C.
 * @param ack Bit de acuse de recibo. 
 *        - 1: Envía ACK (reconocimiento) -> indica al esclavo que envíe el siguiente byte.
 *        - 0: Envía NACK (no reconocimiento) -> indica al esclavo que termine la transmisión.
 * @return El byte recuperado del bus I2C.
 * @warning La convención es: ack = 1 para continuar leyendo, ack = 0 para leer el último byte.
 * @note Originalmente utilizado para lecturas extendidas (ej. BME280/MAX30102).
 * @see I2C_Start, I2C_Restart
 */
unsigned char I2C_Read(unsigned char ack) {
    unsigned char temp;
    I2C_Ready();
    SSPCON2bits.RCEN = 1; 
    while(!SSPSTATbits.BF);
    temp = SSPBUF;
    I2C_Ready();
    SSPCON2bits.ACKDT = (ack) ? 0 : 1; 
    SSPCON2bits.ACKEN = 1;
    return temp;
}

/**
 * @brief Genera la condición de RESTART en el bus I2C.
 * @details Utilizado cuando se desea cambiar la dirección de escritura a lectura
 * sin soltar el bus mediante un comando STOP.
 * @see I2C_Start, I2C_Stop
 */
void I2C_Restart(void) {
    I2C_Ready();
    SSPCON2bits.RSEN = 1;
    while(SSPCON2bits.RSEN);
}

/** @} */ // Fin de I2C_Impl

/* =========================================
 * FUNCIONES OLED
 * ========================================= */

/**
 * @defgroup OLED_Impl Implementación de Funciones OLED
 * @brief Código de alto nivel para el control de la pantalla SSD1306.
 * @{
 */

/**
 * @brief Envía un comando de control al controlador de la pantalla OLED (SSD1306).
 * @param cmd Byte de comando a ejecutar (ver hoja de datos del SSD1306).
 * @see OLED_Dato
 */
void OLED_Comando(unsigned char cmd){
    I2C_Start(OLED_ADDR);
    I2C_Write(OLED_CMD); 
    I2C_Write(cmd); 
    I2C_Stop();
}

/**
 * @brief Envía un byte de datos de píxel a la memoria GRAM de la pantalla OLED.
 * @param dato Byte con el patrón de píxeles a dibujar (1 = encendido, 0 = apagado).
 * @note Este byte se escribe directamente en la memoria de la pantalla en la posición actual del cursor.
 * @see OLED_Comando, OLED_SetCursor
 */
void OLED_Dato(unsigned char dato){
    I2C_Start(OLED_ADDR);
    I2C_Write(OLED_DATA);
    I2C_Write(dato); 
    I2C_Stop();
}

/**
 * @brief Limpia por completo la pantalla OLED rellenando la memoria con 0x00 (píxeles apagados).
 * @details Recorre las 8 páginas y las 128 columnas del display sobrescribiendo datos vacíos.
 * @post Toda la pantalla se muestra en negro (apagada).
 */
void OLED_Clear(void){

    unsigned char i; /**< Página actual */
    unsigned char j; /**< Columna actual */

    for(i = 0; i < 8; i++){
        OLED_Comando(0xB0 | i);
        OLED_Comando(0x00);
        OLED_Comando(0x10);

        for(j = 0; j < 128; j++){
            OLED_Dato(0x00);
        }
    }
}

/**
 * @brief Posiciona el cursor de la pantalla OLED en una ubicación específica.
 * @param pagina Fila lógica de la pantalla (0 a 7). Cada página tiene 8 píxeles de alto.
 * @param col Columna horizontal (0 a 127).
 * @note La combinación de página y columna determina dónde se escribirá el siguiente dato.
 */
void OLED_SetCursor(unsigned char pagina, unsigned char col){
    OLED_Comando(0xB0 | pagina);
    OLED_Comando(col & 0x0F);
    OLED_Comando(0x10 | ((col >> 4) & 0x0F));
}

/**
 * @brief Ejecuta la secuencia de inicialización del controlador SSD1306 de la pantalla OLED.
 * @details Envía los parámetros de multiplexado, reloj, mapeo de segmentos, contraste
 * y enciende finalmente el display. Se ejecuta un barrido de limpieza al final.
 * @post La pantalla queda encendida y lista para mostrar gráficos/texto.
 */
void OLED_Init(void){
    __delay_ms(100);
    OLED_Comando(0xAE); // Display OFF
    OLED_Comando(0xD5);
    OLED_Comando(0x80); // Clock
    OLED_Comando(0xA8);
    OLED_Comando(0x3F); // Multiplex
    OLED_Comando(0xD3);
    OLED_Comando(0x00); // Offset
    OLED_Comando(0x40); // Start line
    OLED_Comando(0x8D); 
    OLED_Comando(0x14); // Charge pump
    OLED_Comando(0x20); 
    OLED_Comando(0x00); // Memory mode
    OLED_Comando(0xA1); // Segment remap
    OLED_Comando(0xC8); // COM scan direction
    OLED_Comando(0xDA); 
    OLED_Comando(0x12); // COM pins
    OLED_Comando(0x81); 
    OLED_Comando(0xCF); // Contrast
    OLED_Comando(0xD9); 
    OLED_Comando(0xF1); // Pre-charge
    OLED_Comando(0xDB);
    OLED_Comando(0x40); // VCOM detect
    OLED_Comando(0xA4); // Resume to RAM content
    OLED_Comando(0xA6); // Normal display (no inverted)
    OLED_Comando(0xAF); // Display ON

    __delay_ms(10);
    OLED_Clear(); 
}

/**
 * @brief Dibuja un solo carácter en la posición actual del cursor de la pantalla.
 * @param c Carácter ASCII a dibujar (rango 32 a 126).
 * @details Transforma el código ASCII buscando sus píxeles en la matriz de fuente 5x8.
 *          Si recibe un carácter fuera del límite soportado, dibuja un espacio.
 * @post El cursor avanza 6 columnas (5 del carácter + 1 de separación).
 * @see OLED_String, font5x8
 */
void OLED_Char(unsigned char c){

    unsigned char i; /**< Contador de columnas del carácter (0 a 4) */

    if(c < 32 || c > 126) c = 32;

    for(i = 0; i < 5; i++){
        OLED_Dato(font5x8[c - 32][i]);
    }

    OLED_Dato(0x00);
}

/**
 * @brief Escribe una cadena de texto completa en la pantalla OLED.
 * @param pagina Fila inicial donde comenzará a dibujarse el texto (0 a 7).
 * @param col Columna horizontal inicial (0 a 127).
 * @param texto Puntero a la cadena de caracteres (debe terminar en nulo '\0').
 * @post El cursor se posiciona al final del texto escrito.
 * @see OLED_Char, OLED_SetCursor
 */
void OLED_String(unsigned char pagina, unsigned char col, const char *texto){
    OLED_SetCursor(pagina, col);
    while(*texto != '\0'){
        OLED_Char(*texto);
        texto++;
    }
}

/** @} */ // Fin de OLED_Impl