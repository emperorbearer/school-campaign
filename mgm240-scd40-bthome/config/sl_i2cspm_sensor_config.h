#ifndef SL_I2CSPM_SENSOR_CONFIG_H
#define SL_I2CSPM_SENSOR_CONFIG_H

// <<< Use Configuration Wizard in Studio >>>

// <h> I2C Bus Settings

// <o SL_I2CSPM_SENSOR_REFERENCE_CLOCK> Reference clock [Hz] (0 = HFPERCLK)
// <i> Default: 0
#define SL_I2CSPM_SENSOR_REFERENCE_CLOCK  0

// <o SL_I2CSPM_SENSOR_SPEED_MODE> Bus speed
// <0=> Standard  (100 kbps)
// <1=> Fast      (400 kbps)
// <2=> Fast Plus (1 Mbps)
// <i> Default: 0
#define SL_I2CSPM_SENSOR_SPEED_MODE  0

// </h>

// <<< end of configuration section >>>

// -----------------------------------------------------------------------
// Pin assignment — adjust to match your hardware layout
// Default: MGM240 Explorer Kit (SLTB010A) EXP header I2C pins
// SDA = PC00, SCL = PC01
// -----------------------------------------------------------------------
#define SL_I2CSPM_SENSOR_PERIPHERAL     I2C0
#define SL_I2CSPM_SENSOR_PERIPHERAL_NO  0

#define SL_I2CSPM_SENSOR_SDA_PORT  gpioPortC
#define SL_I2CSPM_SENSOR_SDA_PIN   0

#define SL_I2CSPM_SENSOR_SCL_PORT  gpioPortC
#define SL_I2CSPM_SENSOR_SCL_PIN   1

#endif // SL_I2CSPM_SENSOR_CONFIG_H
