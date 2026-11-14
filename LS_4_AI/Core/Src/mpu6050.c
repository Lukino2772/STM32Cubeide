#include "mpu6050.h"

/* Write one byte to an MPU6050 register */
static HAL_StatusTypeDef MPU6050_WriteRegister(
    I2C_HandleTypeDef *hi2c,
    uint8_t reg,
    uint8_t value)
{
    return HAL_I2C_Mem_Write(
        hi2c,
        MPU6050_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &value,
        1,
        100
    );
}

/* Initialize MPU6050 */
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = 0;

    /* Check sensor identity */
    if (HAL_I2C_Mem_Read(
        hi2c,
        MPU6050_ADDR,
        MPU6050_WHO_AM_I,
        I2C_MEMADD_SIZE_8BIT,
        &id,
        1,
        100
    ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Expected WHO_AM_I value is 0x68 */
    if (id != 0x68 && id != 0x70 && id != 0x71)
        return HAL_ERROR;

    /* Wake up the sensor */
    if (MPU6050_WriteRegister(
        hi2c, MPU6050_PWR_MGMT_1, 0x01) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(100);

    /* Sample rate: 50 Hz with DLPF enabled */
    if (MPU6050_WriteRegister(
        hi2c, MPU6050_SMPLRT_DIV, 19) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Digital low-pass filter */
    if (MPU6050_WriteRegister(
        hi2c, MPU6050_CONFIG, 0x03) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Accelerometer range: +/- 2g */
    if (MPU6050_WriteRegister(
        hi2c, MPU6050_ACCEL_CONFIG, 0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* Read accelerometer X, Y and Z */
HAL_StatusTypeDef MPU6050_ReadAccel(
    I2C_HandleTypeDef *hi2c,
    MPU6050_Accel_t *accel)
{
    uint8_t data[6];

    if (HAL_I2C_Mem_Read(
        hi2c,
        MPU6050_ADDR,
        MPU6050_ACCEL_XOUT_H,
        I2C_MEMADD_SIZE_8BIT,
        data,
        6,
        100
    ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    accel->x = (int16_t)((data[0] << 8) | data[1]);
    accel->y = (int16_t)((data[2] << 8) | data[3]);
    accel->z = (int16_t)((data[4] << 8) | data[5]);

    return HAL_OK;
}
