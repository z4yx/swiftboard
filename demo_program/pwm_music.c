#include "pwm.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

const int note[][2]={ 
#include "tone.h"
};
int NotenumToHz(int n)
{
	if(n==0) return 0;
	return 440.0*pow(2.0,(n-49)/12.0);
}
int main(int argc, char const *argv[])
{
	char *ptr;
	int fd, mmap_size, hz, duty, i;
	int prescalar, period;

	if(geteuid()){
		printf("%s\n", "Need to be root");
		return 1;
	}

	fd = open("/dev/mem", O_RDWR);
	if(fd < 0){
		perror("open");
		return 1;
	}

	mmap_size = (CONFIG_REG_END_ADDR-CONFIG_REG_START_ADDR);
	ptr = (char*)mmap(0, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CONFIG_REG_START_ADDR);

	if(ptr == MAP_FAILED){
		perror("mmap");
		return 1;
	}

	ACCESS_REG(ptr,PI_CFG0) &= ~PI3_CFG_MASK;
	ACCESS_REG(ptr,PI_CFG0) |= PI3_CFG_PWM1;

	for(i=0; note[i][0]!=-1; i++){
		if(note[i][1] == 0){
			duty = 0;
		}else{
			duty = 50;
			hz = NotenumToHz(note[i][1]);
			if(hz < 196 || hz > 100000){
				printf("Invalid Tone\n");
				continue;
			}
		}
		printf("pwm: %d %d\n", hz, duty);
		if(hz < 261){
			prescalar = 4; //div 480
			period = 50*1000/hz;
		}else if(hz < 392){
			prescalar = 3; //div 360
			period = 66666/hz;
		}else if(hz < 786){
			prescalar = 2; //div 240
			period = 100*1000/hz;
		}else{
			prescalar = 0; //div 120
			period = 200*1000/hz;
		}
		ACCESS_REG(ptr,PWM_CH1_PERIOD) = 
			PWM_CH1_ACT_CYC((int)((double)duty/100*period))|PWM_CH1_ENTIRE_CYC(period-1);
		ACCESS_REG(ptr,PWM_CTRL) &= ~PWM_CH1_MASK;
		ACCESS_REG(ptr,PWM_CTRL) |= 
			PWM_CH1_PRESCALAR(prescalar)|PWM_CH1_EN(1)|
			PWM_CH1_ACT_STATE(1)|PWM_CH1_MODE(0)|PWM_CH1_CLK_GATING(1);
		msync(ptr, mmap_size, MS_SYNC);

		usleep(note[i][0]*1000);
	}
	ACCESS_REG(ptr,PWM_CTRL) &= ~PWM_CH1_MASK;

	ACCESS_REG(ptr,PI_CFG0) &= ~PI3_CFG_MASK;

	munmap(ptr, mmap_size);
	close(fd);

	return 0;
}
