/*
 * imu.c
 *
 *  Created on: Feb 18, 2025
 *      Author: halvard
 * 
 *  Description: imu functions
 */

#include "imu.h"
#include "driver_mpu6050_basic.h"
#include "util.h"

int imu_init(imu_t *imu)
{
    zeromem(imu, sizeof(imu_t));

    if(mpu6050_basic_init(MPU6050_ADDRESS_AD0_LOW) != 0)
    {
        print("MPU6050 init failed!\r\n");
        return 1;
    }
    print("MPU6050 ok\r\n");
    return 0;
}
int imu_process(imu_t *imu)
{
    float* acc = imu->acc;
    float* gyr = imu->gyr;
    if(mpu6050_basic_read(acc, gyr) != 0)
    {
        print("MPU6050 read failed!\r\n");
        return 1;
    }

    // convert to mps2 and map to NED frame
    float accel[3] = {acc[0], acc[1], acc[2]};
    float gyro[3] = {gyr[0], gyr[1], gyr[2]};

    acc[0] = accel[1] * 9.81f;
    acc[1] = accel[0] * 9.81f;
    acc[2] = accel[2] * 9.81f;
    gyr[0] = gyro[1];
    gyr[1] = gyro[0];
    gyr[2] = gyro[2];
    return 0;
}

void imu_deinit(void)
{
    mpu6050_basic_deinit();
}