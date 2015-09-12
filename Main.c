#include "STC15F2K60S2.H"

/**********************
引脚别名定义
***********************/
sbit LED_SEL = P2^3;						//数码管和LED的选择信号
sbit beep = P3^4;								//蜂鸣器
sbit KEY1 = P3^2;								//按键1  温度减
sbit KEY2 = P3^3;								//按键2	 温度加
sbit KEY3 = P1^7;								//按键3  切换模式


/**********************
全局变量定义
***********************/
unsigned int receive_buf = 11;				//接收缓冲，用于收发标志0xca的判断
unsigned int receive_data = 20;				//接受的数据
		
unsigned int temp=20;									//实际温度
unsigned int tempset=30;							//显示设定温度
unsigned int tempset_h=30;						//高温设定
unsigned int tempset_l=10;						//低温设定
unsigned int mode=1;									//高低温显示,1为高温,0为低温
unsigned int Status=1;										//警报状态,1为正常状态,0为警报状态

unsigned int position = 1;			//数码管显示位
unsigned char segtable[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71,0x76,0x38};//段选
unsigned char weixuan[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07};//位选
/**********************
函数声明(中断函数除外)
***********************/
void TimerInit();			//定时器设置
void UartInit();			//串口1设置
void init();					//初始化，推挽，设置中断开关
void Delay(int n);

/********************************
 * 延时函数
********************************/
void Delay(int n)
{
	int y;
	while(n--)
	{
		y=60;
		while(y--);
	}
}

/********************************
 * 设置推挽，中断开关设置
********************************/
void init()
{
	//设置推挽输出
	P0M1 = 0x00;
	P0M0 = 0xff;
	P2M1 = 0x00;
	P2M0 = 0x08;
	P3M1 = 0x00;
	P3M0 = 0x10;		
	ET1 = 0;			//禁止T1中断
	ET0 = 1;			//打开定时器T0中断
	ES = 1;				//打开串口1中断
	IE2 = 0X04;		//打开定时器2中断
	EA = 1;				//打开总中断
}

/********************************
 * 串口1的相关设置
********************************/
void UartInit()
{	
	PCON &= 0x7F;		//波特率不倍速，SMOD=0
	SCON = 0x50;		//串口1使用工作方式1，REN=1(允许串行接收)
	AUXR &= 0xFE;		//串口1选择定时器T1作为波特率发生器，S1ST2=0
	AUXR1 = 0x40;		//串口1在P3.6接收，在P3.7发送
	PS = 1;					//设置串口中断为最高优先级
}

/********************************
 * 定时器的相关设置
********************************/
void TimerInit()	
{
	AUXR |= 0x40;		//定时器T1为1T模式，速度是传统8051的12倍，不分频。
	TMOD &= 0x0F;		//清除T1模式位
	TMOD |= 0x20;		//设置T1模式位，使用8位自动重装模式
	TL1 = 0x70;			//设置初值
	TH1 = 0x70;			//设置T1重装值
	TR1 = 1;				//T1运行控制位置1，允许T1计数
	
	AUXR |= 0x80;		//定时器T0为1T模式，的速度是传统8051的12倍，不分频。
	TMOD &= 0xF0;		//设置定时器模式为16位自动重装
	TL0 = 0xCD;			//设置定时初值
	TH0 = 0xF4;			//设置定时初值
	TF0 = 0;				//清除TF0标志
	TF0 = 0;				//T0溢出标志位清零
	TR0 = 1;				//T0运行控制位置1，允许T0计数
	
//定时器T2为12T模式，用于数码管显示
	T2L = (65536-2500)%256;			//低位重装值
	T2H = (65536-2500)/256;			//高位重装值
	AUXR |= 0x10;		//定时器2开始计时
}




/********************************
 * 定时器0中断的操作，用于蜂鸣器
********************************/
void Time0() interrupt 1
{
	if(Status&&(temp>=tempset_h||temp<=tempset_l))
		beep=~beep;
}

/********************************
* 串口1中断的操作。接收完毕RI值1，产生中断
********************************/
void URAT1() interrupt 4
{
	if(RI)							//判断是否接收中断
	{
		RI = 0;						//接收中断请求标志位清0
		if(receive_buf == 0xca)	//判断上一次是否收到0xca标志
		{
			receive_data = SBUF;	//正式接收数据
			if(receive_data/10<10) //抛弃一部分异常数据
				temp=receive_data;
		}
		receive_buf = SBUF;			//把这次接收到的数据存入自定义的缓存中，等待下一次的比较
	}
}

/********************************
* 定时器2中断的操作。用于显示数码管和LED灯
********************************/
void Timer2() interrupt 12
{
	LED_SEL=0;
	position++;
	if(position==8) 
		position=0;
	P2=weixuan[position];
	switch(position)
	{
		case 1:P0=segtable[temp/10];break; 
		case 2:P0=segtable[temp%10];break; 
		case 4:P0=(mode?segtable[16]:segtable[17]);break; 
		case 5:P0=segtable[tempset%1000/100];break; 
		case 6:P0=segtable[tempset%100/10];break; 
		case 7:P0=segtable[tempset%10];break; 
		default:P0=0x00;break; 
	}	
	Delay(10);
	P0=0x00;
	
	if(Status&&(temp>=tempset_h||temp<=tempset_l))//处于报警状态时交替显示数码管和LED灯
	{
		LED_SEL=1;
		P0=0xff;
		Delay(1);
		P0=0x00;
		LED_SEL=0;
	}
}


/********************************
 * 主函数
********************************/
void main()
{
	TimerInit();		//设置定时器
	UartInit();			//设置串口1
	init();					//初始化
	while(1){
	if(KEY2==0)  //警报温度+1
	{
		Delay(20);
		if(KEY2==0)
		{
			while(KEY2==0);
			if(mode) 
				tempset_h+=1;
			else 
				tempset_l+=1;
		}
	}
	if(KEY1==0)  //警报温度-1
	{
		Delay(20);
		if(KEY1==0)
		{
			while(KEY1==0);
			if(mode) 
				tempset_h-=1;
			else 
				tempset_l-=1;
		}
	}
	if(KEY3==0)  //高低温切换
	{
		Delay(20);
		if(KEY3==0)
		{
			while(KEY3==0);
			if(Status&&(temp>=tempset_h||temp<=tempset_l)) 
				Status=0;
			else
				mode=!mode;	
		} //改变高低温状态变量
	}
	if(mode)     //改变设定温度
		tempset=tempset_h;
	else 
		tempset=tempset_l;
	if(temp<tempset_h&&temp>tempset_l) //改变报警状态
		Status=1; 
	}
}

