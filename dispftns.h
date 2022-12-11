#ifndef _GRAYSATLMT
#define _GRAYSATLMT 30
#endif

#define _THM_FIRE  0
#define _THM_RAINB 1
#define _THM_FEVR  2
#define _THM_ROOM  3
#define _THM_AMB   4
#define _THM_HUE   5
#define _THM_BLEU  6
#define _THM_TIV   7
#define _THM_LAVA  8
#define _THM_MAX   7        // maximum value so camera knows when to cycle

float degc_from_reading(int reading);
float degf_from_reading(int reading);
int sgn(const float f);
unsigned char* rgb_from_temp(int temp);
unsigned char* rgb_from_temp_fever(int temp);
unsigned char* rgb_in_minmax(float reading, float min, float max);
unsigned char* fire_grad(float reading, float mn, float mx);
unsigned char* lava_grad(float reading, float mn, float mx);
unsigned char* bleu_grad(float reading, float mn, float mx);
unsigned char* rgb_from_temp_rainbow(float reading, float min, float max);

unsigned char* centered_grad(float reading, float mn, float mx, float cen);

int rgb256(float r, float g, float b);

char* thumb_name(char* inpfn);

#define _THERMCOLORS 15
#define _FEVCOLORS 8
// const unsigned int  _therm_level[_THERMCOLORS];
