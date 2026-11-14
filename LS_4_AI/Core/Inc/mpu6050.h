#ifndef MPU6050_H
#define MPU6050_H

#include "main.h"
#include <stdint.h>

/* MPU6050 I2C address */
#define MPU6050_ADDR          (0x68 << 1)

/* MPU6050 registers */
#define MPU6050_WHO_AM_I      0x75
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_SMPLRT_DIV    0x19
#define MPU6050_CONFIG        0x1A
#define MPU6050_ACCEL_CONFIG  0x1C
#define MPU6050_ACCEL_XOUT_H  0x3B

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} MPU6050_Accel_t;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef MPU6050_ReadAccel(
    I2C_HandleTypeDef *hi2c,
    MPU6050_Accel_t *accel
);

#endif
