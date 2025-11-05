/*
 * kalman.h
 *
 *  Created on: Oct 29, 2025
 *      Author: halva
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "imu.h"

void kalman_init(float dt);
void kalman_update(imu_t* imu);
float kalman_get_angle(void); // rad
float kalman_get_bias(void); //rad/s

#endif /* INC_KALMAN_H_ */
