#define _BSD_SOURCE   
#define _XOPEN_SOURCE

          
#include <unistd.h>				// for I2C port
#include <fcntl.h>				// for I2C port
#include <sys/ioctl.h>			// for I2C port
// #include <linux/i2c.h>          // for I2C port
#include <linux/i2c-dev.h>		// for I2C port

#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */

#include <stdint.h>
#include "mlx90640-library/headers/MLX90640_API.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "i2cctl.h"
#include "dispftns.h"


// #define _AVG_PLAID

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

#define PATH_MAX 1024

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
     

#define IMGMAPMAX 1080

#define _1STORDER_BLUR 0.33
#define MLX_I2C_ADDR 0x33

// Valid frame rates are 1, 2, 4, 8, 16, 32 and 64
// The i2c baudrate is set to 1mhz to support these
#define FPS 16
#define _SLEEPMS 350
#define FRAME_TIME_MICROS (400000/FPS)


void move_cursor(int x, int y)
{	printf("\x1b[%i;%if", y, x);	
}


int locopid, lastkill;

int main(char argc, char** argv)
{
	int file_i2c;
	int length;
	unsigned char massive_buffer[65536] = {0};

	unsigned int mypid = getpid();
	char cmdbuf[1000];
	char path[PATH_MAX];
	sprintf(cmdbuf, "ps -e | grep thermcam2 | grep -v %d", mypid);
	FILE *pf = popen(cmdbuf, "r");
	while (fgets(path, PATH_MAX, pf) != NULL)
	{   pclose(pf);
	printf("thermcam2 already running\n");
	return -1;
	}
	
	sprintf(cmdbuf, "sudo renice -n -17 %i", mypid);
    system(cmdbuf);
    
    

	key_t key = ftok("/tmp/shm",70);
	int shmid  = shmget(key, 2048*sizeof(int), 0666 | IPC_CREAT);
	int *thermdat = (int*)shmat(shmid, (void*)0, 0);



	unsigned int address = 0x33;

	
	int i, j, k;
	int noprint = 0;
	

	// file_i2c = i2copen(address); if (file_i2c == -1) return 1;

	
	unsigned int r, g, b;
	unsigned long avg, maxv, minv;
	float min_degc, min_degf, max_degc, max_degf;
	int iter=0, maxiter = 10000;
	
	
	if (argc > 1) maxiter = atoi(argv[1]);
	
	if (argc > 2)
	{   for (i=2; i<argc; i++)
	    {   if (!strcmp(argv[i], "-nop")) noprint++;
	    }
	}
	

    static uint16_t eeMLX90640[832];
	float emissivity = 0.8;
	uint16_t frame[834];
	// static char image[IMAGE_SIZE];
	static float mlx90640To[768];
	float eTa;
	static uint16_t data[768*sizeof(float)];
	static int fps = FPS;
	static long frame_time_micros = FRAME_TIME_MICROS;
	char *p;


/*
	writereg16(file_i2c, 0x800D, 0x1D81);
	writereg16(file_i2c, 0x2430, 0x18EF);
	*/
	MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b011);
	MLX90640_SetChessMode(MLX_I2C_ADDR);

    paramsMLX90640 mlx90640;
    MLX90640_DumpEE(MLX_I2C_ADDR, eeMLX90640);
    MLX90640_SetResolution(MLX_I2C_ADDR, 0x03);
	
	if (!noprint) system("clear");
	    
    	MLX90640_ExtractParameters(eeMLX90640, &mlx90640);

		MLX90640_SetDeviceMode(MLX_I2C_ADDR, 0);
		MLX90640_SetSubPageRepeat(MLX_I2C_ADDR, 0);

	while (1)
	{	move_cursor(1,1);
		

	    
        MLX90640_GetFrameData(MLX_I2C_ADDR, frame);
        MLX90640_InterpolateOutliers(frame, eeMLX90640);

        eTa = MLX90640_GetTa(frame, &mlx90640); // Sensor ambient temprature
        MLX90640_CalculateTo(frame, &mlx90640, emissivity, eTa, mlx90640To); //calculate temprature of all pixels, base on emissivity of object

		/*float th_avg = 0;
		int px_avg = 0;*/
		int mammal = 0;
		int max = -99998;
        for(int y = 0; y < 24; y++)
        {   for(int x = 0; x < 32; x++)
            {   float val = mlx90640To[32 * (23-y) + x];
                if (fabs(val)>0.01) 
                {	thermdat[x+32*y] = (int)(val*3+64);
                	/*th_avg += thermdat[x+32*y];
                	px_avg++;*/
                }
            }
        }
        
        thermdat[1023] = eTa * 4;
        
		




		// auto frame_time = std::chrono::microseconds(frame_time_micros + OFFSET_MICROS);

//		MLX90640_SetDeviceMode(MLX_I2C_ADDR, 0);
//		MLX90640_SetSubPageRepeat(MLX_I2C_ADDR, 0);
	   
            
            

#if 1
            // Blur
            for (int y=0; y<24; y++)
            {   int y1 = (24-y)*32;
                for (int x=0; x<32; x++)
                {   i = y1 + x;
                    
                    int i8 = i - 32;
                    int i4 = i - 1;
                    int i6 = i + 1;
                    int i2 = i + 32;
                    
                    float w8 = 1;
                    j = thermdat[i];
                    
                    if (i8 >= 0)
                    {   j += _1STORDER_BLUR * thermdat[i8];
                        w8 += _1STORDER_BLUR;
                    }
                    
                    if (i4 >= 0)
                    {   j += _1STORDER_BLUR * thermdat[i4];
                        w8 += _1STORDER_BLUR;
                    }
                    
                    if (i6 < 768)
                    {   j += _1STORDER_BLUR * thermdat[i6];
                        w8 += _1STORDER_BLUR;
                    }
                    
                    if (i2 < 768)
                    {   j += _1STORDER_BLUR * thermdat[i2];
                        w8 += _1STORDER_BLUR;
                    }
                    
                    j /= w8;
                    thermdat[i] = j;
                }
            }
#endif

			int thavg = 0;
			if (!noprint)
			{
			for (i=0; i<768; i++)
			{   
			    j = thermdat[i];
                // j &= 0xFFFF;
                thavg += j;
                
            	// if (j > 105 && j < 160) mammal++;
            	if (j > max) max = j;
			
			    char* rgb = rgb_from_temp(j);
				if (!noprint) printf("\x1b[48;5;%im  \x1b[0m", rgb256(rgb[0],rgb[1],rgb[2]));
				
				if (!noprint && (i%32)==31) printf("\n");
			}
			thavg /= 768;
			}
			if (!noprint) printf("\n");
			if (!noprint) printf("Average: %i\n", thavg);
			
			
			if (!noprint) printf("\n");
		
			// writereg16(file_i2c, 0x8000, k & 0xfff7);
		
			
			if (!noprint) printf("\n");
			iter++;
			if (iter >= maxiter) break;
			
		
		iter++;
		if (iter > maxiter) break;
    	
    	
    	usleep(_SLEEPMS*1000);  
    }
    
    // printf("\n");

    return 0;
}
