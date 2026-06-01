#ifndef MAX30102_LIBRERIA_H
#define MAX30102_LIBRERIA_H

#include <xc.h>
#include "OLED_Libreria.h"

#ifndef _XTAL_FREQ
#define _XTAL_FREQ 8000000
#endif

/* Direccion I2C del MAX30102 en formato de 8 bits (7 bits + bit R/W)
 * La direccion de 7 bits es fija: 0x57
 *   0x57 << 1 | 0 = 0xAE  (escritura)
 *   0x57 << 1 | 1 = 0xAF  (lectura)  */
#define MAX30102_ADDR_W   0xAE
#define MAX30102_ADDR_R   0xAF

/* Registros internos del MAX30102 */
#define MAX_REG_INT_STATUS1   0x00
#define MAX_REG_INT_STATUS2   0x01
#define MAX_REG_INT_ENABLE1   0x02
#define MAX_REG_INT_ENABLE2   0x03
#define MAX_REG_FIFO_WR_PTR   0x04   /* El sensor escribe muestras aqui         */
#define MAX_REG_OVF_COUNTER   0x05   /* Cuenta muestras perdidas por FIFO llena */
#define MAX_REG_FIFO_RD_PTR   0x06   /* El PIC lee muestras desde aqui          */
#define MAX_REG_FIFO_DATA     0x07   /* Puerto de salida de datos de la FIFO    */
#define MAX_REG_FIFO_CONFIG   0x08
#define MAX_REG_MODE_CONFIG   0x09
#define MAX_REG_SPO2_CONFIG   0x0A
#define MAX_REG_LED1_PA       0x0C   /* Corriente del LED Rojo (0x00=0mA, 0xFF=51mA) */
#define MAX_REG_LED2_PA       0x0D   /* Corriente del LED IR   (apagado en modo HR)   */
#define MAX_REG_TEMP_INT      0x1F
#define MAX_REG_TEMP_FRAC     0x20
#define MAX_REG_TEMP_CONFIG   0x21
#define MAX_REG_REV_ID        0xFE
#define MAX_REG_PART_ID       0xFF   /* Siempre vale 0x15 en el MAX30102 */

/* Promediado de muestras en la FIFO (campo SMP_AVE, bits [7:5] de FIFO_CONFIG)
 * Promediar reduce el ruido pero tambien la frecuencia de actualizacion */
#define MAX_SMP_AVE_1    0x00
#define MAX_SMP_AVE_2    0x20
#define MAX_SMP_AVE_4    0x40
#define MAX_SMP_AVE_8    0x60
#define MAX_SMP_AVE_16   0x80
#define MAX_SMP_AVE_32   0xA0

/* Modos de operacion (campo MODE, bits [2:0] de MODE_CONFIG)
 * HR    -> solo LED Rojo activo, 3 bytes por muestra en FIFO
 * SPO2  -> LED Rojo + IR activos, 6 bytes por muestra en FIFO
 * MULTI -> configuracion avanzada, no usada en este proyecto */
#define MAX_MODE_HR      0x02
#define MAX_MODE_SPO2    0x03
#define MAX_MODE_MULTI   0x07

/* Rango maximo del ADC (campo SPO2_ADC_RGE, bits [6:5] de SPO2_CONFIG)
 * A mayor rango, menos sensibilidad pero mas margen ante señales fuertes */
#define MAX_ADC_RGE_2048  0x00
#define MAX_ADC_RGE_4096  0x20
#define MAX_ADC_RGE_8192  0x40
#define MAX_ADC_RGE_16384 0x60

/* Frecuencia de muestreo (campo SPO2_SR, bits [4:2] de SPO2_CONFIG)
 * 100 SPS es suficiente para detectar pulso cardiaco (rango humano: 40-200 LPM) */
#define MAX_SR_50    0x00
#define MAX_SR_100   0x04
#define MAX_SR_200   0x08
#define MAX_SR_400   0x0C
#define MAX_SR_800   0x10
#define MAX_SR_1000  0x14
#define MAX_SR_1600  0x18
#define MAX_SR_3200  0x1C

/* Ancho de pulso del LED (campo LED_PW, bits [1:0] de SPO2_CONFIG)
 * A mayor ancho de pulso, mas luz emitida y mayor resolucion del ADC
 * 411 us -> 18 bits de resolucion (valor maximo) */
#define MAX_PW_69    0x00
#define MAX_PW_118   0x01
#define MAX_PW_215   0x02
#define MAX_PW_411   0x03

/* Corrientes predefinidas para los LEDs (1 LSB = 0.2 mA)
 * Mayor corriente = señal mas fuerte, pero mas consumo de bateria */
#define MAX_LED_LOW    0x1F   /* ~6.4  mA */
#define MAX_LED_MED    0x47   /* ~14.2 mA */
#define MAX_LED_HIGH   0x7F   /* ~25.4 mA */
#define MAX_LED_MAX    0xFF   /* ~51.0 mA */

/* Estructura que contiene una muestra leida de la FIFO.
 * En modo HR solo se usa 'red'. El campo 'ir' siempre vale 0
 * porque el LED IR esta apagado. Si en el futuro se cambia a
 * modo SPO2, hay que actualizar ReadSample() para leer ambos canales. */
typedef struct {
    unsigned long red;
    unsigned long ir;
} MAX30102_Sample;

/* Prototipos */
void          MAX30102_Init(void);
void          MAX30102_Reset(void);
void          MAX30102_Shutdown(void);
void          MAX30102_Wakeup(void);
unsigned char MAX30102_ReadReg(unsigned char reg);
void          MAX30102_WriteReg(unsigned char reg, unsigned char val);
void          MAX30102_ClearFIFO(void);
unsigned char MAX30102_SamplesAvailable(void);
unsigned char MAX30102_ReadSample(MAX30102_Sample *s);
signed int    MAX30102_ReadTempRaw(void);
unsigned char MAX30102_GetPartID(void);
unsigned char MAX30102_IsConnected(void);
unsigned char MAX30102_ProcesarBPM(unsigned long raw_red, unsigned char *bpm_out);

#endif /* MAX30102_LIBRERIA_H */