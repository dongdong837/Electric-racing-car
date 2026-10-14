#ifndef _TASK_H_
#define _TASK_H_

#include "main.h"

typedef struct {
    float kp;
    float ki;
    float kd;
    float out_p[2];
    float out_i[2];
    float out_d[2];
    float error[2];
    float pre_error[2];
	int max_speed;
	int max_rotate[2];
    int negative[2];
} pid_group;

// Bresenham直线路径
typedef struct {
    int start_x, start_y;   // 起点坐标
    int end_x, end_y;       // 终点坐标
    int cur_x, cur_y; // 当前点坐标
    int dx, dy;             // X/Y方向距离
    int sx, sy;             // X/Y方向符号
    int err;                // 误差值
    uint8_t active;         // 路径是否激活
} BresenhamPath;

void SetAngley(float Angle2);
void SetAnglex(float Angle1);
void datatqu1(void) ;
void datatqu2(void) ;
void Incre_PID(pid_group* pid, int *Error, int *Angle);
void Bresenham_Init(BresenhamPath *path, int x0, int y0, int x1, int y1);
uint8_t Bresenham_Next(BresenhamPath *path);

#endif
