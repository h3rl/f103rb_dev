/*
 * kalman.h
 *
 *  Created on: Nov 5, 2025
 *      Author: halva
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "imu.h"
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the 2D Kalman filter (pitch + roll, with biases) */
void kalman_init(float dt);

/** Perform one Kalman update using the latest IMU data */
void kalman_update(imu_t* imu);

/** Get the current estimated pitch angle [radians] */
float kalman_get_pitch(void);

/** Get the current estimated roll angle [radians] */
float kalman_get_roll(void);

/** Get the current estimated pitch bias [radians/s] */
float kalman_get_pitch_bias(void);

/** Get the current estimated roll bias [radians/s] */
float kalman_get_roll_bias(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_KALMAN_H_ */
