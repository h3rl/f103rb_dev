#include "kalman.h"
#include <math.h>
#include <stdbool.h>

static bool kalman_ready = false;
static float dt;

/* State */
static float x[4];       // [pitch, pitch_bias, roll, roll_bias]
static float P[4][4];    // covariance

/* Process and measurement noise */
static const float q_angle = 5e-3f;  // faster angle response (increase for faster angle response but it will cost noise!)
static const float q_bias  = 1e-6f;  // allow bias to adapt slowly
static const float r_meas  = 5e-3f;  // accelerometer trusted moderately

void kalman_init(float dt_)
{
    dt = dt_;
    for(int i=0;i<4;i++) x[i]=0.0f;
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            P[i][j] = (i==j) ? 1e-1f : 0.0f;
    kalman_ready = true;
}

void kalman_update(imu_t* imu)
{
    if(!kalman_ready) return;

    // Convert gyro to rad/s
    float gx = imu->gyr[0]*M_PI/180.0f;
    float gy = imu->gyr[1]*M_PI/180.0f;

    // Accelerometer pitch/roll (radians)
    float pitch_acc = atan2f(-imu->acc[0], sqrtf(imu->acc[1]*imu->acc[1] + imu->acc[2]*imu->acc[2]));
    float roll_acc  = atan2f(imu->acc[1], imu->acc[2]);

    // --- Prediction ---
    float x_pred[4];
    x_pred[0] = x[0] + dt*(gx - x[1]);
    x_pred[1] = x[1];
    x_pred[2] = x[2] + dt*(gy - x[3]);
    x_pred[3] = x[3];

    // --- Covariance prediction ---
    float P_pred[4][4];
    // P00
    P_pred[0][0] = P[0][0] + dt*(dt*P[1][1] - P[0][1] - P[1][0] + q_angle);
    P_pred[0][1] = P[0][1] - dt*P[1][1];
    P_pred[0][2] = P[0][2] - dt*P[1][3];
    P_pred[0][3] = P[0][3] - dt*P[1][3]; // cross-axis small, optional

    P_pred[1][0] = P[1][0] - dt*P[1][1];
    P_pred[1][1] = P[1][1] + q_bias*dt;
    P_pred[1][2] = P[1][2];
    P_pred[1][3] = P[1][3];

    P_pred[2][0] = P[2][0] - dt*P[3][0];
    P_pred[2][1] = P[2][1];
    P_pred[2][2] = P[2][2] + dt*(dt*P[3][3] - P[2][3] - P[3][2] + q_angle);
    P_pred[2][3] = P[2][3] - dt*P[3][3];

    P_pred[3][0] = P[3][0];
    P_pred[3][1] = P[3][1];
    P_pred[3][2] = P[3][2] - dt*P[3][3];
    P_pred[3][3] = P[3][3] + q_bias*dt;

    // --- Innovation ---
    float y[2] = { pitch_acc - x_pred[0], roll_acc - x_pred[2] };

    // --- Innovation covariance S (2x2) ---
    float S[2][2];
    S[0][0] = P_pred[0][0] + r_meas;
    S[0][1] = P_pred[0][2];
    S[1][0] = P_pred[2][0];
    S[1][1] = P_pred[2][2] + r_meas;

    // --- Invert S ---
    float det = S[0][0]*S[1][1] - S[0][1]*S[1][0];
    if(fabsf(det)<1e-12f) det = 1e-12f;
    float S_inv[2][2];
    S_inv[0][0] =  S[1][1]/det;
    S_inv[0][1] = -S[0][1]/det;
    S_inv[1][0] = -S[1][0]/det;
    S_inv[1][1] =  S[0][0]/det;

    // --- Kalman gain K = P_pred * C' * S^-1 ---
    float K[4][2];
    K[0][0] = P_pred[0][0]*S_inv[0][0] + P_pred[0][2]*S_inv[1][0];
    K[0][1] = P_pred[0][0]*S_inv[0][1] + P_pred[0][2]*S_inv[1][1];
    K[1][0] = P_pred[1][0]*S_inv[0][0] + P_pred[1][2]*S_inv[1][0];
    K[1][1] = P_pred[1][0]*S_inv[0][1] + P_pred[1][2]*S_inv[1][1];
    K[2][0] = P_pred[2][0]*S_inv[0][0] + P_pred[2][2]*S_inv[1][0];
    K[2][1] = P_pred[2][0]*S_inv[0][1] + P_pred[2][2]*S_inv[1][1];
    K[3][0] = P_pred[3][0]*S_inv[0][0] + P_pred[3][2]*S_inv[1][0];
    K[3][1] = P_pred[3][0]*S_inv[0][1] + P_pred[3][2]*S_inv[1][1];

    // --- State update ---
    for(int i=0;i<4;i++)
        x[i] = x_pred[i] + K[i][0]*y[0] + K[i][1]*y[1];

    // --- Covariance update P = (I-K*C)*P_pred ---
    float P_new[4][4];
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            P_new[i][j] = P_pred[i][j]
                        - K[i][0]*P_pred[0][j]
                        - K[i][1]*P_pred[2][j];

    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            P[i][j] = P_new[i][j];
}

// --- Getters ---
float kalman_get_pitch(void)       { return x[0]; }
float kalman_get_pitch_bias(void)  { return x[1]; }
float kalman_get_roll(void)        { return x[2]; }
float kalman_get_roll_bias(void)   { return x[3]; }
