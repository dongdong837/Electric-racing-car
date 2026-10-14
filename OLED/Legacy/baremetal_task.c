#include "task.h"

#define Err 0


extern uint8_t rxdata[22];

extern uint16_t Get_x[4],Get_y[4];
extern uint8_t shape;
extern uint16_t jiaoyan;
extern uint16_t current_x,current_y;
extern uint8_t flag;

int calcu_tran[2];
float cal_time = CYCLE_TIME; // it should be better when set to 10 for PID
float calcu_result[2]={0,0};

extern BresenhamPath laser_path;   


void SetAngley(float Angle2)
{
	int pwm;
	pwm=Angle2  * 500 / 180 + 500;
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_2,pwm);
}
void SetAnglex(float Angle1)
{
	int pwm;
	pwm=Angle1  * 500 / 180 + 500;
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,pwm);
}


void datatqu1(void) 
{
    if(rxdata[0] == 0x24 && rxdata[21] == 0x23) //检测头尾
	{
        uint16_t sum = 0;
        for(int i = 1; i < 20; i++)//十九个求和 
		{ sum += rxdata[i];}
        jiaoyan = (uint8_t)(sum & 0xFF);//底八位校验
        if(jiaoyan == rxdata[20]) {
			shape = rxdata[3];
			for(int i=0;i<4;i++)
			{
				Get_x[i] = rxdata[4+i*4]  | (rxdata[5+i*4] << 8);
				Get_y[i] = rxdata[6+i*4]  | (rxdata[7+i*4] << 8);
			}
        } 
    } 
}

void datatqu2(void) 
{
    if(rxdata[0] == 0x24 && rxdata[10] == 0x23) //检测头尾
	{
		uint16_t sum = 0;
        for(int i = 1; i < 9; i++)//十九个求和 
		{ sum += rxdata[i];}
        jiaoyan = (uint8_t)(sum & 0xFF);//底八位校验
        if(jiaoyan == rxdata[9]) 
		{
			current_x= rxdata[5]  | (rxdata[6] << 8);
			current_y = rxdata[7]  | (rxdata[8] << 8);
		}
    } 
}


// 增量式PID控制器
void Incre_PID(pid_group* pid, int *Error, int *Angle) 
{
    for (int i = 0; i < 2; i++) // 0=X轴, 1=Y轴
	{  
        int temp = Error[i];
        pid->error[i] = (temp >= -Err && temp <= Err) ? 0 : temp;
        
        pid->out_p[i] = pid->kp * pid->error[i];
        pid->out_d[i] = pid->kd * (pid->error[i] - pid->pre_error[i]);
        
        if (abs(temp) <= 20) {
            pid->out_i[i] += pid->ki * pid->error[i];
            pid->out_i[i] = MINMAX(pid->out_i[i], -500, 500);  // 积分限幅
        }
        
        float out_val = pid->out_p[i] + pid->out_i[i] + pid->out_d[i];
        
        out_val *= pid->negative[i];
        
//        int speed_limit = (abs(Error[0]) < 50 && abs(Error[1]) < 50) 
//                         ? pid->max_speed : 3;
//        out_val = MINMAX(out_val, -speed_limit, speed_limit);
        
        int new_angle = Angle[i] + (int)out_val;
        Angle[i] = MINMAX(new_angle, 
                            pid->max_rotate[0], 
                            pid->max_rotate[1]);
        
        pid->pre_error[i] = pid->error[i];
    }
}



// 初始化路径
void Bresenham_Init(BresenhamPath *path, int x0, int y0, int x1, int y1) {
    path->start_x = x0;
    path->start_y = y0;
    path->cur_x = x0;
    path->cur_y = y0;
    path->end_x = x1;
    path->end_y = y1;
    
    path->dx = abs(x1 - x0);
    path->dy = abs(y1 - y0);
    path->sx = (x0 < x1) ? 1 : -1;
    path->sy = (y0 < y1) ? 1 : -1;
    path->err = path->dx - path->dy;
    path->active = 1;
	
//	path->current_x =x0;
//	path->current_y =y0;

}

// 获取路径上的下一个点
uint8_t Bresenham_Next(BresenhamPath *path) {
    if (!path->active) return 0;
    
    // 到达终点
    if (path->cur_x == path->end_x && path->cur_y == path->end_y) {
        path->active = 0;
        return 0;
    }
    
    int e2 = 2 * path->err;
    if (e2 > -path->dy) {
        path->err -= path->dy;
        path->cur_x += path->sx;
    }
    if (e2 < path->dx) {
        path->err += path->dx;
        path->cur_y += path->sy;
    }
    
    return 1;
}




