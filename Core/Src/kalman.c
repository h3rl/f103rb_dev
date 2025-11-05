/*
 * kalman.c
 *
 *  Created on: Oct 29, 2025
 *      Author: halva
 */

#include "kalman.h"
#include "arm_math.h"
#include <math.h>
#include <stdbool.h>

typedef arm_matrix_instance_f64 mat_t; // kept for compatibility

#define NX 2   // states: [angle, bias]
#define NU 1   // inputs: gyro rate
#define NY 1   // measurement: accel pitch

/* persistent state */
static bool kalman_ready = false;

/* storage */
static float32_t A_data[NX*NX];
static float32_t B_data[NX*NU];
static float32_t C_data[NY*NX];
static float32_t Q_data[NX*NX];
static float32_t R_data[NY*NY];
static float32_t xhat_data[NX*1];
static float32_t P_data[NX*NX];

/* matrix instances */
static arm_matrix_instance_f32 A, B, C, Q, R, xhat, P;

/* temporaries */
static float32_t tmp1_data[NX*NX];
static float32_t tmp2_data[NX*NX];
static float32_t tmpv1_data[NX];
static float32_t tmpv2_data[NX];
static arm_matrix_instance_f32 tmp1, tmp2, tmpv1, tmpv2;

void kalman_init(float dt)
{
    if (kalman_ready)
        return;

    /* init matrices */
    arm_mat_init_f32(&A, NX, NX, A_data);
    arm_mat_init_f32(&B, NX, NU, B_data);
    arm_mat_init_f32(&C, NY, NX, C_data);
    arm_mat_init_f32(&Q, NX, NX, Q_data);
    arm_mat_init_f32(&R, NY, NY, R_data);
    arm_mat_init_f32(&xhat, NX, 1, xhat_data);
    arm_mat_init_f32(&P, NX, NX, P_data);
    arm_mat_init_f32(&tmp1, NX, NX, tmp1_data);
    arm_mat_init_f32(&tmp2, NX, NX, tmp2_data);
    arm_mat_init_f32(&tmpv1, NX, 1, tmpv1_data);
    arm_mat_init_f32(&tmpv2, NX, 1, tmpv2_data);

    /* constants */
    const float32_t q_angle = 1e-4f;
    const float32_t q_bias  = 1e-6f;
    const float32_t r_meas  = 1e-2f;

    /* model:
       angle_k+1 = angle_k + (gyro - bias)*dt
       bias_k+1  = bias_k
       y = angle
    */
    A_data[0] = 1.0f;  A_data[1] = -dt;
    A_data[2] = 0.0f;  A_data[3] = 1.0f;

    B_data[0] = dt;
    B_data[1] = 0.0f;

    C_data[0] = 1.0f;  C_data[1] = 0.0f;

    Q_data[0] = q_angle; Q_data[1] = 0.0f;
    Q_data[2] = 0.0f;    Q_data[3] = q_bias;

    R_data[0] = r_meas;

    xhat_data[0] = 0.0f;
    xhat_data[1] = 0.0f;

    P_data[0] = 1e-1f; P_data[1] = 0.0f;
    P_data[2] = 0.0f;  P_data[3] = 1e-1f;

    kalman_ready = true;
}

void kalman_update(imu_t* imu)
{
    if (!kalman_ready)
    {
    	assert("kalman not initialized" && 0);
    }

    /* Convert gyro X (deg/s) -> rad/s */
    const float32_t gyro_rate = imu->gyr[0] * (float32_t)M_PI / 180.0f;

    /* Pitch from accelerometer */
    float32_t ax = imu->acc[0];
    float32_t ay = imu->acc[1];
    float32_t az = imu->acc[2];
    float32_t denom = sqrtf(ay*ay + az*az);
    if (denom < 1e-6f) denom = 1e-6f;
    float32_t y_meas = atan2f(-ax, denom);

    /* ---- Prediction ---- */
    // x_pred = A*xhat + B*u
    arm_mat_mult_f32(&A, &xhat, &tmpv1);       // tmpv1 = A*xhat
    tmpv1_data[0] += B_data[0] * gyro_rate;    // add B*u
    tmpv1_data[1] += B_data[1] * gyro_rate;

    // P_pred = A*P*A' + Q
    arm_mat_mult_f32(&A, &P, &tmp1);
    arm_mat_trans_f32(&A, &tmp2);
    arm_mat_mult_f32(&tmp1, &tmp2, &tmp1);
    arm_mat_add_f32(&tmp1, &Q, &tmp1);

    /* ---- Update ---- */
    // S = C*P_pred*C' + R  (scalar here)
    float32_t S;
    float32_t CtPC;
    CtPC = tmp1_data[0]*C_data[0]*C_data[0] +
           tmp1_data[1]*C_data[0]*C_data[1] +
           tmp1_data[2]*C_data[1]*C_data[0] +
           tmp1_data[3]*C_data[1]*C_data[1];
    S = CtPC + R_data[0];
    float32_t invS = 1.0f / S;

    // K = P_pred*C'*inv(S)
    float32_t K0 = (tmp1_data[0]*C_data[0] + tmp1_data[1]*C_data[1]) * invS;
    float32_t K1 = (tmp1_data[2]*C_data[0] + tmp1_data[3]*C_data[1]) * invS;

    // innovation = y - C*x_pred
    float32_t y_pred = C_data[0]*tmpv1_data[0] + C_data[1]*tmpv1_data[1];
    float32_t innov = y_meas - y_pred;

    // xhat = x_pred + K*innov
    xhat_data[0] = tmpv1_data[0] + K0 * innov;
    xhat_data[1] = tmpv1_data[1] + K1 * innov;

    // P = (I - K*C)*P_pred
    float32_t I_KC[4];
    I_KC[0] = 1.0f - K0*C_data[0];
    I_KC[1] = -K0*C_data[1];
    I_KC[2] = -K1*C_data[0];
    I_KC[3] = 1.0f - K1*C_data[1];

    tmp2_data[0] = I_KC[0]*tmp1_data[0] + I_KC[1]*tmp1_data[2];
    tmp2_data[1] = I_KC[0]*tmp1_data[1] + I_KC[1]*tmp1_data[3];
    tmp2_data[2] = I_KC[2]*tmp1_data[0] + I_KC[3]*tmp1_data[2];
    tmp2_data[3] = I_KC[2]*tmp1_data[1] + I_KC[3]*tmp1_data[3];

    P_data[0] = tmp2_data[0];
    P_data[1] = tmp2_data[1];
    P_data[2] = tmp2_data[2];
    P_data[3] = tmp2_data[3];
}

float kalman_get_angle(void)
{
    return xhat_data[0];   // estimated pitch angle [radians]
}

float kalman_get_bias(void)
{
    return xhat_data[1];   // estimated gyro bias [radians/s]
}

