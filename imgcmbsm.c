#define _XOPEN_SOURCE

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <jpeglib.h>    
#include <jerror.h>
#include <png.h>

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

#include "mkbmp.h"
#include "dispftns.h"

// Thermal camera selection (default without any defines = AMG8833)
#define _MLX90640

// Added lightness for thermal overlay
#define _thermlite 255

#define _USE_5D_THERMCMB 0

#define _USE_JPEG_MINMAX 0

// RGB factor for 5D thermal-optical alignment
// TODO: make this depend on spacing constants also make this a one-time global for performance reasons
#define rgbf 0.0007

// darkness compensation for hues. 
// Increase if night shots are too infrared; 
// decrease if monochrome illumination looks washed out.
#define iauc 0.01

#ifdef _MLX90640

#define _THERM_W 32
#define _THERM_H 24

// Thermcam offset to align to NoIR - multiply these by wid, hei
float therm_off_x =     0.215;
float therm_off_y =     0.15;

// Thermcam is mounted crooked because I'm under the weather and drunk
float therm_rot_rad =   2.0 * 3.1415926535897932384626 / 180;

// NoIR size per thermcam 16x16-pixel block - multiply these by wid, hei
#define therm_sz_x 0.0213
#define therm_sz_y 0.0285

// Half-size for new thermal squares to be drawn
#define sq_hsz 4

#else

#define _THERM_W 8
#define _THERM_H 8

// Thermcam offset to align to NoIR - multiply these by wid, hei
#define therm_off_x 0.17
#define therm_off_y 0.05

// NoIR size per thermcam 16x16-pixel block - multiply these by wid, hei
#define therm_sz_x 0.10
#define therm_sz_y 0.12

// Half-size for new thermal squares to be drawn
#define sq_hsz 10

#endif

// Eye offset to align to NoIR - multiply these by wid, hei
#define eye_off_x 0.02
#define eye_off_y -.125

// NoIR size per eye pixel - multiply these by wid, hei
#define eye_sz_x 0.195
#define eye_sz_y 0.25

#define line_spacing 81
#define line_width 8

// Mappings
#define _rgi 0x659
#define _rig 0x695
#define _gir 0x596
#define _gri 0x569
#define _irg 0x965
#define _igr 0x956

int histogram[10000];
int histmx = 0;

int therm_mode = _THM_HUE;

typedef struct
{
    unsigned int x;
    unsigned int y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
}
pixel_5d;


typedef struct
{	unsigned int idx[4];
	float wt[4];
}
wghidx;


float distance_between_pix5(pixel_5d a, pixel_5d b)
{	
	float r, d;
	d = a.x - b.x; r  = d*d;
	d = a.y - b.y; r += d*d;
	
	d = rgbf*(a.r - b.r); r += d*d;
	d = rgbf*(a.g - b.g); r += d*d;
	d = rgbf*(a.b - b.b); r += d*d;
		  
	return pow(r, 0.5);
}

pixel_5d thermspot[_THERM_W*_THERM_H];
unsigned char thermr[_THERM_W*_THERM_H], thermg[_THERM_W*_THERM_H], thermb[_THERM_W*_THERM_H];
int wid, hei, wid2, hei2;
float thmult[780];
unsigned char jprmx, jpgmx, jpbmx, jprmn, jpgmn, jpbmn;

int nearest_thermspot_5d(pixel_5d target)
{	
	int x, y, i, j, k, l;
	float r1, r2;
	
	// First, narrow it down by Y
	r1 = 999999;
	j = -1;
	for (y=0; y<_THERM_H; y++)
	{	i = _THERM_W*y;
		r2 = abs(thermspot[i].y - target.y);
		if (r2 < r1) { r1 = r2; j = y; }
	}
	
	// Then, narrow it down by X
	r1 = 999999;
	k = -1;
	for (x=0; x<_THERM_W; x++)
	{	i = _THERM_W*j + x;
		r2 = abs(thermspot[i].x - target.x);
		if (r2 < r1) { r1 = r2; k = x; }
	}
	
	// Now scan
	r1 = 999999;
	l = k + _THERM_W*j;					// default in case we don't find
	// return l;
	for (y = j-2; y <= j+2; y++)
	{	if (y < 0) y = 0;
		if (y >= _THERM_H) break;
		for (x = k-2; x <= k+2; x++)
		{	if (x<0) x = 0;
			if (x>=_THERM_W) continue;
			
			i = x + _THERM_W*y;
			
			r2 = distance_between_pix5(target, thermspot[i]);
			if (r2 < r1) { r1 = r2; l = i; }
		}
	}
	
	return l;	
}

wghidx blur_thermspot(pixel_5d target)
{	int x, y, i, j, k, l;
	wghidx retval;
	
#if _USE_5D_THERMCMB
	int cx0, cx1, cy0, cy1;
	int htx, hty;
	
	htx = (wid*therm_sz_x);
	hty = (hei*therm_sz_y);
	
	cx0 = target.x - htx;
	cx1 = target.x + htx;
	cy0 = target.y - hty;
	cy1 = target.y + hty;
	
	int q1, q2, q3, q4;
	
	target.x = cx0;
	target.y = cy0;
	retval.idx[0] = q1 = nearest_thermspot_5d(target);
	float r1 = distance_between_pix5(target, thermspot[q1]);
	target.x = cx1;
	retval.idx[1] = q2 = nearest_thermspot_5d(target);
	float r2 = distance_between_pix5(target, thermspot[q2]);
	target.y = cy1;
	retval.idx[2] = q3 = nearest_thermspot_5d(target);
	float r3 = distance_between_pix5(target, thermspot[q3]);
	target.y = cy0;
	retval.idx[3] = q4 = nearest_thermspot_5d(target);
	float r4 = distance_between_pix5(target, thermspot[q4]);
	
	r1 = 1.0/(r1+0.001);
	r2 = 1.0/(r2+0.001);
	r3 = 1.0/(r3+0.001);
	r4 = 1.0/(r4+0.001);
	
	float sumr = r1+r2+r3+r4;
	
    retval.wt[0] = r1 / sumr;
    retval.wt[1] = r2 / sumr;
    retval.wt[2] = r3 / sumr;
    retval.wt[3] = r4 / sumr;

	return retval;
	
#else
	
	float r0, r1, r2, r3;	
	float tx, ty;
	
	// Apply the thermcam offset
	tx = target.x - wid*therm_off_x;
	ty = target.y - hei*therm_off_y;
	
	
	// Apply the rotation
	float sr, cr;
	sr = sin(-therm_rot_rad);
	cr = cos(-therm_rot_rad);
	
	tx -= wid2;
	ty -= hei2;
	
	tx = tx * cr - ty * sr;
	ty = ty * cr + tx * sr;

	tx += wid2;
	ty += hei2;
	
	
	// Divide by the therm pixel spacing
	tx /= (wid*therm_sz_x);
	ty /= (hei*therm_sz_y);
	
	
	// Clip the limits
	if (tx < 0) tx = 0;
	if (tx >= _THERM_W) tx = _THERM_W-1;
	if (ty < 0) ty = 0;
	if (ty >= _THERM_H) ty = _THERM_H-1;
	
	
	// Take the floors and set aside the remainders 
	int txi, tyi;
	
	txi = floor(tx);
	tyi = floor(ty);
	
	float dx, dy;
	dx = tx - txi;
	dy = ty - tyi;
	
	
	// TODO: Perform a bicubic resample
	// For now just linear is fine
	retval.idx[0] = txi + _THERM_W*tyi;
	
	retval.idx[1] = (txi < _THERM_W-1)
	              ? retval.idx[0]+1
	              : retval.idx[0];
	              ;
	              
    retval.idx[2] = (tyi < _THERM_H-1)
                  ? retval.idx[0]+_THERM_W
                  : retval.idx[0];
                  ;
                  
    retval.idx[3] = (tyi < _THERM_H-1)
                  ? retval.idx[1]+_THERM_W
                  : retval.idx[1];
                  ;
                  
    // retval.wt[0] = 1; retval.wt[1] = retval.wt[2] = retval.wt[3] = 0; return retval;
	
	r0 = abs(target.r - thermspot[retval.idx[0]].r)
	   + abs(target.g - thermspot[retval.idx[0]].g)
	   + abs(target.b - thermspot[retval.idx[0]].b)
	   ;
	r1 = abs(target.r - thermspot[retval.idx[1]].r)
	   + abs(target.g - thermspot[retval.idx[1]].g)
	   + abs(target.b - thermspot[retval.idx[1]].b)
	   ;
	r2 = abs(target.r - thermspot[retval.idx[2]].r)
	   + abs(target.g - thermspot[retval.idx[2]].g)
	   + abs(target.b - thermspot[retval.idx[2]].b)
	   ;
	r3 = abs(target.r - thermspot[retval.idx[3]].r)
	   + abs(target.g - thermspot[retval.idx[3]].g)
	   + abs(target.b - thermspot[retval.idx[3]].b)
	   ;
	   
	r0 = r0 * rgbf +        dx  *        dy ;
	r1 = r1 * rgbf + (1.0 - dx) *        dy ;
	r2 = r2 * rgbf +        dx  * (1.0 - dy);
	r3 = r3 * rgbf + (1.0 - dx) * (1.0 - dy);
                  
    /*
    retval.wt[0] = (1.0 - dx) * (1.0 - dy);
    retval.wt[1] =        dx  * (1.0 - dy);
    retval.wt[2] = (1.0 - dx) *        dy ;
    retval.wt[3] =        dx  *        dy ;
    */
	
	r1 = 1.0/(r1+0.001);
	r2 = 1.0/(r2+0.001);
	r3 = 1.0/(r3+0.001);
	r0 = 1.0/(r0+0.001);
	
	float sumr = r1+r2+r3+r0;
	
    retval.wt[0] = r0 / sumr;
    retval.wt[1] = r1 / sumr;
    retval.wt[2] = r2 / sumr;
    retval.wt[3] = r3 / sumr;
	
	return retval;
	
#endif
}

char* rgb_from_reading(int reading, int tempmin, int tempmax, int sensortmp)
{   char* rgb;
    
    switch (therm_mode)
	{	case _THM_FIRE:
		rgb = fire_grad(reading, tempmin, tempmax);
		break;
		
		case _THM_LAVA:
		rgb = lava_grad(reading, tempmin, tempmax);
		break;
		
		case _THM_FEVR:
		rgb = rgb_from_temp_fever(reading);
		break;
		
		case _THM_ROOM:
        rgb = centered_grad(reading, 5*4+25, 50*4+25, 25*4+25);
        break;
        
        case _THM_AMB:
        rgb = centered_grad(reading, tempmin-5, tempmax+5, sensortmp);  // thermdat[1023]
        break;
		
		case _THM_RAINB:
		rgb = rgb_in_minmax(reading, tempmin, tempmax);
		break;
		
		case _THM_BLEU:
		case _THM_TIV:
		rgb = bleu_grad(reading, tempmin, tempmax);
		break;
		
		case _THM_HUE:
		default:
		rgb = rgb_from_temp(reading);
		break;
	}
	
	return rgb;
}

/* A coloured pixel. */

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}
pixel_t;

/* A picture. */
    
typedef struct
{
    pixel_t *pixels;
    size_t width;
    size_t height;
}
bitmap_t;
    
/* Given "bitmap", this returns the pixel of bitmap at the point 
   ("x", "y"). */

static pixel_t * pixel_at (bitmap_t * bitmap, int x, int y)
{
    return bitmap->pixels + bitmap->width * y + x;
}
    
/* Write "bitmap" to a PNG file specified by "path"; returns 0 on
   success, non-zero on error. */

static int save_png_to_file (bitmap_t *bitmap, const char *path)
{
    FILE * fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t x, y;
    png_byte ** row_pointers = NULL;
    /* "status" contains the return value of this function. At first
       it is set to a value which means 'failure'. When the routine
       has finished its work, it is set to a value which means
       'success'. */
    int status = -1;
    /* The following number is set by trial and error only. I cannot
       see where it it is documented in the libpng manual.
    */
    int pixel_size = 3;
    int depth = 8;
    
    fp = fopen (path, "wb");
    if (! fp) {
		printf("fopen failed.\n");
        goto fopen_failed;
    }

	// MingW has a version mismatch; hopefully RPi doesn't.
    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
		printf("create write struct failed.\n");
        goto png_create_write_struct_failed;
    }
    
    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
		printf("create info struct failed.\n");
        goto png_create_info_struct_failed;
    }
    
    /* Set up error handling. */

    if (setjmp (png_jmpbuf (png_ptr))) {
		printf("jmpbuf failed.\n");
        goto png_failure;
    }
    
    /* Set image attributes. */

    png_set_IHDR (png_ptr,
                  info_ptr,
                  bitmap->width,
                  bitmap->height,
                  depth,
                  PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);
    
    /* Initialize rows of PNG. */

    row_pointers = png_malloc (png_ptr, bitmap->height * sizeof (png_byte *));
    for (y = 0; y < bitmap->height; y++) {
        png_byte *row = 
            png_malloc (png_ptr, sizeof (uint8_t) * bitmap->width * pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < bitmap->width; x++) {
            pixel_t * pixel = pixel_at (bitmap, x, y);
            *row++ = pixel->red;
            *row++ = pixel->green;
            *row++ = pixel->blue;
        }
    }
    
    /* Write the image data to "fp". */

    png_init_io (png_ptr, fp);
    png_set_rows (png_ptr, info_ptr, row_pointers);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* The routine has successfully written the file, so we set
       "status" to a value which indicates success. */

    status = 0;
    
    for (y = 0; y < bitmap->height; y++) {
        png_free (png_ptr, row_pointers[y]);
    }
    png_free (png_ptr, row_pointers);
    
 png_failure:
 png_create_info_struct_failed:
    png_destroy_write_struct (&png_ptr, &info_ptr);
 png_create_write_struct_failed:
    fclose (fp);
 fopen_failed:
    return status;
}

/* Given "value" and "max", the maximum value which we expect "value"
   to take, this returns an integer between 0 and 255 proportional to
   "value" divided by "max". */

static int pix (int value, int max)
{
    if (value < 0) {
        return 0;
    }
    return (int) (256.0 *((double) (value)/(double) max));
}


unsigned int type;  
unsigned char * rowptr[1];    // pointer to an array
unsigned char * jdata;        // data for the image
struct jpeg_decompress_struct info; //for our jpeg info
struct jpeg_error_mgr err;          //the error handler

unsigned char* LoadJPEG(char* FileName)
//================================
{
  unsigned long x, y;
  unsigned int texture_id;
  unsigned long data_size;     // length of the file
  int channels;               //  3 =>RGB   4 =>RGBA
  
  jprmx = jpgmx = jpbmx = 0;
  jprmn = jpgmn = jpbmn = 255;

  FILE* file = fopen(FileName, "rb");  //open the file

  info.err = jpeg_std_error(& err);     
  jpeg_create_decompress(& info);   //fills info structure

  //if the jpeg file doesn't load
  if(!file) {
     fprintf(stderr, "Error reading JPEG file %s!", FileName);
     return 0;
  }

  jpeg_stdio_src(&info, file);    
  jpeg_read_header(&info, TRUE);   // read jpeg file header

  jpeg_start_decompress(&info);    // decompress the file

  //set width and height
  x = info.output_width;
  y = info.output_height;
  channels = info.num_components;
  //type = GL_RGB;
  //if(channels == 4) type = GL_RGBA;

  data_size = x * y * 3;

  //--------------------------------------------
  // read scanlines one at a time & put bytes 
  //    in jdata[] array. Assumes an RGB image
  //--------------------------------------------
  jdata = (unsigned char *)malloc(data_size);
  int maxx = 3* info.output_width;
  while (info.output_scanline < info.output_height) // loop
  {
    // Enable jpeg_read_scanlines() to fill our jdata array
    rowptr[0] = (unsigned char *)jdata +  // secret to method
            maxx * info.output_scanline; 

    jpeg_read_scanlines(&info, rowptr, 1);
    
    #if _USE_JPEG_MINMAX
    for (int x=0; x<maxx; x+=3)
    {   if (rowptr[0][x] > jprmx) jprmx = rowptr[0][x];
        if (rowptr[0][x] < jprmn) jprmn = rowptr[0][x];
        
        if (rowptr[0][x+1] > jpgmx) jpgmx = rowptr[0][x+1];
        if (rowptr[0][x+1] < jpgmn) jpgmn = rowptr[0][x+1];
        
        if (rowptr[0][x+2] > jpbmx) jpbmx = rowptr[0][x+2];
        if (rowptr[0][x+2] < jpbmn) jpbmn = rowptr[0][x+2];
    }
    #endif
  }
  //---------------------------------------------------

  jpeg_finish_decompress(&info);   //finish decompressing
  
  return jdata;
}


void load_thalign(void)
{   FILE* pf = fopen("/home/pi/thalign", "r");
	if (pf)
	{	char buffer[1024];
		fgets(buffer, 1024, pf);	therm_off_x = atof(buffer);
	    fgets(buffer, 1024, pf);	therm_off_y = atof(buffer);
	    fgets(buffer, 1024, pf);	therm_rot_rad = atof(buffer);
	    fclose(pf);
	}
}
    

int main(char argc, char** argv)
{
	char *in_rgi, *in_therm, *in_eye, *out_comp;
	unsigned char *rdat, *gdat, *bdat;
	unsigned long rtot, gtot, btot;
	unsigned char rmax, gmax, bmax;
	int del_inp_f = 0;
	
	int mypid = getpid();
    char cmdbuf[256];
    // sprintf(cmdbuf, "sudo renice -n +10 -p %i", mypid);
    // system(cmdbuf);
    
    // nice doesn't work - neither does cpulimit, and you have to install it
    // sprintf(cmdbuf, "cpulimit -p %i -n 25", getpid());
    // system(cmdbuf);

	in_rgi = in_therm = in_eye = 0;

#ifdef _MLX90640
	key_t keyt = ftok("/tmp/shm",73);
	int shmidt  = shmget(keyt, 2048*sizeof(int), 0666 | IPC_CREAT);
	int *thermdatr = (int*)shmat(shmidt, (void*)0, 0);
	int *thermdat = malloc(1024*sizeof(int));
	
	for (int i=0; i<768; i++) thermdat[i] = thermdatr[i];
	
#else
	// AMG8833
    key_t keyt = ftok("/tmp/shm",71);
    int shmidt  = shmget(keyt, 80*sizeof(int), 0666 | IPC_CREAT);
    int *thermdat = (int*)shmat(shmidt, (void*)0, 0);
#endif
	
	load_thalign();
	
	char* thdat;
	
	int rotcol = 0, rgb = 0, thermfill = 0; //, fire=0;
	int cmapping = _rgi;
	int bluewhite = 0;
	int found = 0;
	int imono = 0;
	for (int i=1; i<(argc-1); i++)
	{	if (!strcmp(argv[i], "-rgi"  )) { in_rgi   = argv[i+1]; rgb = 0; found++; }
		if (!strcmp(argv[i], "-rgb"  )) { in_rgi   = argv[i+1]; rgb = 1; found++; }
		if (!strcmp(argv[i], "-therm")) { in_therm = "yes"; found++; }
		if (!strcmp(argv[i], "-o"    )) { out_comp = argv[i+1]; 		 }
		
		if (!strcmp(argv[i], "-thdat")) { thdat    = argv[i+1];          }
		
		if (!strcmp(argv[i], "-r"))		  { rotcol = 1; cmapping = _irg; }
		if (!strcmp(argv[i], "-im"))      imono = 1;
		if (!strcmp(argv[i], "-tf"))	  thermfill = 1;
		if (!strcmp(argv[i], "-fire"))	  therm_mode = _THM_FIRE;
		if (!strcmp(argv[i], "-fevr"))	  therm_mode = _THM_FEVR;
		if (!strcmp(argv[i], "-room"))	  therm_mode = _THM_ROOM;
		if (!strcmp(argv[i], "-amb"))	  therm_mode = _THM_AMB;
		if (!strcmp(argv[i], "-rain"))	  therm_mode = _THM_RAINB;
		if (!strcmp(argv[i], "-bleu"))	  therm_mode = _THM_BLEU;
		if (!strcmp(argv[i], "-lava"))	  therm_mode = _THM_LAVA;
		if (!strcmp(argv[i], "-tiv"))	  { therm_mode = _THM_TIV; rotcol = 1; cmapping = _irg; imono = 0; }
		if (!strcmp(argv[i], "-dif"))	  del_inp_f = 1;
		if (!strcmp(argv[i], "-rgi"))	  cmapping = _rgi;
		if (!strcmp(argv[i], "-rig"))	  cmapping = _rig;
		if (!strcmp(argv[i], "-gir"))	  cmapping = _gir;
		if (!strcmp(argv[i], "-gri"))	  cmapping = _gri;
		if (!strcmp(argv[i], "-irg"))	  cmapping = _irg;
		if (!strcmp(argv[i], "-igr"))	  cmapping = _igr;
		if (!strcmp(argv[i], "-bw"))	  { cmapping = _rgi; bluewhite = 1; }
	}
	
	// if (therm_mode == _THM_FIRE) printf("fire\n");
	
	if (!out_comp)
	{	printf("No output file.\nUsage:\nimgcomb -rgi NoIR_input.jpg -therm -o output.bmp\n");
		return 3;
	}
	
	/*if (found < 2)
	{	printf("Nothing to process.\n");
		return 1;
	}*/
	
	#if _USE_JPEG_MINMAX
	// If for any reason no minmax data, then no correction.
	if (jprmx <= jprmn) { jprmx = 255; jprmn = 0; }
	if (jpgmx <= jpgmn) { jpgmx = 255; jpgmn = 0; }
	if (jpbmx <= jpbmn) { jpbmx = 255; jpbmn = 0; }
	
	float rcorr = 255.0 / (jprmx - jprmn);
	float gcorr = 255.0 / (jpgmx - jpgmn);
	float bcorr = 255.0 / (jpbmx - jpbmn);
	
	#endif	
	
	for (int i=0; i<10000; i++) histogram[i] = 0;
	
	if (in_rgi)
	{	LoadJPEG(in_rgi);
		wid = info.output_width;
		hei = info.output_height;
		wid2 = wid/2;
		hei2 = hei/2;
		
		rtot = gtot = btot = wid*hei*16;
		rmax = gmax = bmax = 0;
		
		int rmin = 255;
		
		int perline = wid*3, pixels = wid*hei;
		
		rdat = (unsigned char *)malloc(pixels);
		gdat = (unsigned char *)malloc(pixels);
		bdat = (unsigned char *)malloc(pixels);
		
#if 1
        float r,g,b;
		unsigned long satttl = 1, lumttl = 1;
		
		for (unsigned int y=0; y<hei; y+=16)
		{	int ly = y * perline;
			int by = y * wid;
			for (unsigned int x=0; x<wid; x+=16)
			{	int lx = x * 3;
				int bx = by+x;
				
				r = /* rdat[bx] =*/ jdata[ly+x*3  ];
				g = /* gdat[bx] =*/ jdata[ly+x*3+1];
				b = /* bdat[bx] =*/ jdata[ly+x*3+2];
				
				#if _USE_JPEG_MINMAX
				/*r = (r - jprmn) * rcorr;
				g = (g - jpgmn) * gcorr;
				b = (b - jpbmn) * bcorr;*/
				#endif
				
				satttl += fabs(r-g) + fabs(g-b) + fabs(r-b);
				lumttl += fmax(r, fmax(g, b));
			}
		}
				
		float satcorr = 0.5*(float)lumttl / satttl;
		if (satcorr > 0.75) satcorr = 0.75;
		
		
		for (unsigned int y=0; y<hei; y++)
		{	int ly = y * perline;
			int by = y * wid;
			for (unsigned int x=0; x<wid; x++)
			{	int lx = x * 3;
				int bx = by+x;
				
				
				
				
				r = /* rdat[bx] =*/ jdata[ly+x*3  ];
				g = /* gdat[bx] =*/ jdata[ly+x*3+1];
				b = /* bdat[bx] =*/ jdata[ly+x*3+2];
				
				#if _USE_JPEG_MINMAX
				r = (r - jprmn) * rcorr;
				g = (g - jpgmn) * gcorr;
				b = (b - jpbmn) * bcorr;
				#endif
				
				int rg = 99.9 * (float)r     / (r+g  );
				int yb = 49.9 * (float)(r+g) / (r+g+b);
				
				histogram[rg + 100*yb]++;
				if (histogram[rg + 100*yb] > histmx) histmx = histogram[rg + 100*yb];
				
				if (imono)
				{	float a = 0.3 * r + 0.3 * g + 0.4 * b;
					if (a < b) b = a;
					r = g = b;
				}
				else if (hei < 800)				// no enhancement in 1080p for performance reasons.
                {   
                    /*r = 255 * pow(0.00390625*r, 1.08);
                    g = 255 * pow(0.00390625*g, 1.03);
                    b = 255 * pow(0.00390625*b, 1.33);*/
                    
				    // r *= 0.95; g *= 0.95; b *= 0.95;
				    
				    
				    
				    // Correct for IR paleness
				    if (!rgb)
				    {	b *= 1.5;

					    /*if (b > r)*/ r -= 0.8*satcorr*(b - r);
				    	// else r -= 0.4 * b;
                                        
					    /*if (b > g)*/ g -= 0.8*satcorr*(b - g);
				    	// else g -= 0.4 * b;
				    	
				    	b *= .67;
				    } 
				    
				    
				    // Correct purpling at high brightness
				    if (b > 192)
				    {   r += (b - 192);
				        g += 2*(b - 192);
				        
				        if (r > 255) r = 255;
				        if (g > 255) g = 255;
				    }
				    
				    
				    // RG saturation
			        float rg = (r+1)/(g+1);
			        if (rg < 0.5) rg = 0.5;
			        if (rg > 2  ) rg = 2  ;
			        float rgsat = (rg > 1) ? 0.63 : 0.77;
			        
			        rgsat *= satcorr;
			        
			        // rgsat *= .005*(g-56);
			        
			        rg = pow(rg, 0.333);
			        r = rgsat*(rg*r) + (1.0-rgsat)*r;
			        g = rgsat*(g/rg) + (1.0-rgsat)*g;
			        
				    
				    // if (r<0) r=0;
				    // if (g<0) g=0;
				    
				    
				    /*
				    // Decrease Y-B saturation
				    int yb = (0.5*rdat[bx]+0.5*gdat[bx]) - bdat[bx];
				    rdat[bx] -= 0.25 * yb;
				    gdat[bx] -= 0.25 * yb; 
				    bdat[bx] += 0.25 * yb;
				    */
				    
				    // Increase G-B saturation
				    int yb = (0.2*rdat[bx]+0.9*gdat[bx]) - bdat[bx];
				    rdat[bx] += 0.25 * yb;
				    gdat[bx] += 0.25 * yb; 
				    bdat[bx] -= 0.33 * yb;

				    
				    r = 0.5 * r 
				      + 0.5 * (g < b ? g : b);
				    
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
				    
				    /*
				    // Increase green gamma
				    float gam = .00392 * g;
				    gam = pow(gam, 0.91);
				    g = 255.0 * gam;
				    
				    // Increase red gamma
				    gam = .00392 * r;
				    gam = pow(gam, 0.94);
				    r = 255.0 * gam;
				    */
				    
				    // Don't decrease blue gamma
				    /*
				    if (!rgb)
				    {	gam = .00392 * b;
					    gam = pow(gam, 1.25);
					    b = 255.0 * gam;
				    }*/
			    }			
				
			    

				int plum;			// pixel lum, pronounced "plume"
				float r_l, g_l, b_l;
				
				if (cmapping != _rgi)
				{   // plum = 0.30 * r + 0.56 * g + 0.14 * b;
					plum = 0.40 * r + 0.60 * g;
					
					r_l = r - plum;
					g_l = g - plum;
					b_l = (b - plum)*.75;
					
					if (therm_mode == _THM_TIV && b_l > 0) b_l /= 2;
				}
				
				switch (cmapping)
				{   
    				case _irg:
					rdat[bx] = plum + b_l;
				    gdat[bx] = plum + r_l;
				    bdat[bx] = plum + g_l;
				    break;
				    
				    case _igr:
					rdat[bx] = plum + b_l;
				    gdat[bx] = plum + g_l;
				    bdat[bx] = plum + r_l;
				    break;
				    
				    case _rig:
				    
				    /*
				    r_l *= .00625;
				    g_l *= .00625;
				    b_l *= .00625;
				    
				    if (r_l > 0)  r_l = pow(r_l, 0.81);
				    else r_l = -pow(-r_l, 0.81);
				    
				    if (g_l > 0)  g_l = pow(g_l, 0.67);
				    else g_l = -pow(-g_l, 0.67);
				    
				    if (b_l > 0)  b_l = pow(b_l, 0.78);
				    else b_l = -pow(-b_l, 0.78);
				    
				    
				    r_l *= 160;
				    g_l *= 160;
				    b_l *= 160;
				    */
				    
				    r_l *= 1.25;
				    g_l *= 1.33;
				    b_l *= 1.5;

					r = plum + r_l;
				    g = plum + b_l;
				    b = plum + g_l;
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;

				    
					rdat[bx] = r; //plum + r_l;
				    gdat[bx] = g; //plum + b_l;
				    bdat[bx] = b; //plum + g_l;
				    
				    if (rdat[bx] > gdat[bx] && bdat[bx] > gdat[bx])
				    {   gdat[bx] += 0.5*(rdat[bx] - gdat[bx]);
				        gdat[bx] += 0.5*(bdat[bx] - gdat[bx]);
				    }
				    break;
				    
				    case _gir:
					rdat[bx] = plum + g_l;
				    gdat[bx] = plum + b_l;
				    bdat[bx] = plum + r_l;
				    break;
				    
				    case _gri:
				    
				    if (b_l < g_l)
				        r_l = 0.5 * r_l + 0.5 * b_l;
				    
				    if (b_l < 0) g_l -= b_l;
				    if (g_l > 0) 
				    {   g_l *= 2.5;
				    }
				    
					r = plum + g_l;
				    g = plum + r_l;
				    b = plum + b_l;				    
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
				    
					rdat[bx] = r;
				    gdat[bx] = g;
				    bdat[bx] = b;
				    
				    
				    
				    
				    break;
				    
				    default:
				    
				    rdat[bx] = r;
				    gdat[bx] = g;
				    bdat[bx] = b;
				}
				
				rtot += rdat[bx];
				gtot += gdat[bx];
				btot += bdat[bx];
				
				if (rdat[bx] > rmax) rmax = rdat[bx];
				if (gdat[bx] > gmax) gmax = gdat[bx];
				if (bdat[bx] > bmax) bmax = bdat[bx];
				
				// if (rdat[bx] < rmin) rmin = rdat[bx];
			}
		}
		
		printf("RGB totals: %i %i %i\n", rtot, gtot, btot);
		
		
		float rgam, ggam, bgam, rmul, gmul, bmul;
		rgam = ggam = bgam = 1;
		int totavg = (rtot + gtot + btot) /3;
		
		// rmax -= rmin;
		
		if (!totavg) totavg++;
		
		if (!rmax) rmax++;
		if (!gmax) gmax++;
		if (!bmax) bmax++;
		
		rmul = (rmax) ? (255.0 / rmax) : 1;
		gmul = (gmax) ? (255.0 / gmax) : 1;
		bmul = (bmax) ? (255.0 / bmax) : 1;
		
		
		rgam = pow((float)rtot / totavg, 0.53);
		ggam = pow((float)gtot / totavg, 0.53);
		bgam = pow((float)btot / totavg, 0.53);
		
		if (0)
		{	rgam /= 0.67;
			ggam /= 0.67;
			bgam /= 0.67;
		}
		
		printf("rgam ggam bgam: %f %f %f\n", rgam, ggam, bgam);
		/* if (rotcol)
		{   rgam /= 1.13;
		    ggam /= 0.71;
		    bgam /= 0.67;
		}
		else
		{   bgam /= 1.13;
		    rgam /= 0.71;
		    ggam /= 0.67;
		} */
		
		printf("rgam ggam bgam: %f %f %f\n", rgam, ggam, bgam);
		
		float desat = 0;
		
		if (hei < 800)
        {
		    for (unsigned int y=0; y<hei; y++)
		    {	int ly = y * perline;
			    int by = y * wid;
			    for (unsigned int x=0; x<wid; x++)
			    {	int lx = x * 3;
				    int bx = by+x;
				    
				    // rdat[bx] -= rmin;
				    
				    rdat[bx] = 246*pow(.00391 * rmul * rdat[bx], rgam);
				    gdat[bx] = 246*pow(.00391 * gmul * gdat[bx], ggam);
				    bdat[bx] = 246*pow(.00391 * bmul * bdat[bx], bgam);
				    
			        if (bluewhite)
			        {   
			            r = rdat[bx];
			            g = gdat[bx];
			            b = bdat[bx];
			            
			            // if (r > b || g > b) r = g = b;
			            if (r > b || g > b) r = g = b;
			            else
			            {   r = g = 0.5*r + 0.5*g;
			            }
			            
			            rdat[bx] = r;
			            gdat[bx] = g;
			            bdat[bx] = b;
			        }
			        
			        if (cmapping == _igr)
			        {   r = rdat[bx];
			            g = gdat[bx];
			            b = bdat[bx];
			            
			            if (b > g) g += 0.5*(b-g);
			            
			            rdat[bx] = r;
			            gdat[bx] = g;
			            bdat[bx] = b;
			        }
			    
			    }
		    }
		}
#endif
		
	}
	else
	{	printf("NoIR-less merge not supported!\n");
		return 2;
	}
	
	
	
	
	
	int maxby = wid*(hei-2);
	int reading;
	
	if (in_therm && 1)
	{	unsigned char rtmp[16*16*8*8], gtmp[16*16*8*8], btmp[16*16*8*8];
		unsigned int wtmp = _THERM_W, htmp = _THERM_H;
		
		FILE* fp;
		if (thdat && thermdat)
		{   if (fp = fopen(thdat, "rb"))
		    {   fread(thermdat, 768, sizeof(int), fp);
		        fread(thermdat+1023, 1, sizeof(int), fp);
		        fclose(fp);
		    }
		}
		
		
		
		if (thermdat)
		{	int tempmin = 0, tempmax = 0, frist = 1;
		
		    int i;
    
            for (i=0; i<768; i++) thmult[i] = 1;
	        
            FILE* pf = fopen("/home/pi/thmult.dat", "r");
            if (pf)
            {   char buffer[1024];
                for (i=0; i<768; i++)
                {   fgets(buffer, 1024, pf);
                    thmult[i] = atof(buffer);
                    if ( !thmult[i] ) thmult[i] = 1;
                }
                fclose(pf);
            }
	
		
#ifdef _MLX90640
			for (int y=0; y<_THERM_H; y++)
			{	int ly = y * _THERM_W;
				for (int x=0; x<_THERM_W; x++)
				{	int lx = ly+x;
					
					int reading = thermdat[lx]; // & 0xffff;
					reading *= thmult[lx];
					/*if (reading & 0x8000) reading -= 0x10000;
					reading = 0.4*reading + 160;*/

#else
			for (int y=8; y<128; y+=16)
			{	int ly = ((y-8) >> 4) * wtmp;
				for (int x=8; x<128; x+=16)
				{	int lx = ly+(x>>4);
				
					reading = thermdat[lx] & 0xfff;
					if (reading & 0x800) reading -= 0x1000;
#endif
					if (frist || (reading < tempmin)) tempmin = reading;
					if (frist || (reading > tempmax)) tempmax = reading;
					frist = 0;
				}
			}
		
#ifdef _MLX90640
			int hei2 = hei/2;
			int wid2 = wid/2;
			int x, y, lx, ly;
			
			float tharr[_THERM_W+4];
			float frarr[_THERM_W+4];
			float fgarr[_THERM_W+4];
			float fbarr[_THERM_W+4];
			
			int focus_line = _THERM_H / 2;      // TODO
			int maybe_parallax;
			int best_parallax = therm_off_x;
			float best_correlation = 0;
			
			ly = y * wtmp;
			for (maybe_parallax = 0; maybe_parallax < _THERM_W/2; maybe_parallax++)
			{
			    for (x=0; x<_THERM_W; x++)
			    {
        			lx = ly+x;
				    reading = thermdat[lx]; 
				    tharr[x] = reading;
				    
				    int bx = maybe_parallax * wid + therm_sz_x * wid * x;
				    int by = ((int)(therm_off_y * hei) + (int)(therm_sz_y * hei * y));
				    
				    int tsi = x+_THERM_W*y;
				    int bmi = bx + wid*by;
				    
				    frarr[x] = rdat[bmi];
				    fgarr[x] = gdat[bmi];
				    fbarr[x] = bdat[bmi];
			    }
			}
			
			tempmin -= 2;
			tempmax += 2;

			for (int y=0; y<_THERM_H; y++)
			{	int ly = y * wtmp;
				
				// printf("Processing line %i offset %i...\n", y, by);
				
				for (int x=0; x<_THERM_W; x++)
				{	int bx = therm_off_x * wid + therm_sz_x * wid * x;
					int by = ((int)(therm_off_y * hei) + (int)(therm_sz_y * hei * y));
				
					int by1 = hei2 + (by-hei2) * cos(therm_rot_rad) + (bx-wid2) * sin(therm_rot_rad);
					int bx1 = wid2 + (bx-wid2) * cos(therm_rot_rad) - (by-hei2) * sin(therm_rot_rad);
					
					by = by1;
					bx = bx1;
					
					
					int tsi = x+_THERM_W*y;
					int bmi = bx + wid*by;
					thermspot[tsi].x = bx;
					thermspot[tsi].y = by;
					thermspot[tsi].r = rdat[bmi];
					thermspot[tsi].g = gdat[bmi];
					thermspot[tsi].b = bdat[bmi];
					
				
					int lx = ly+x;
					reading = thermdat[lx]; 
					reading *= thmult[lx];
					char *rgb;
								   
					// therm_mode == _THM_FIRE
					rgb = rgb_from_reading(reading, tempmin, tempmax, thermdat[1023]);
					
					thermr[tsi] = rgb[0];
					thermg[tsi] = rgb[1];
					thermb[tsi] = rgb[2];
#else
			for (int y=8; y<128; y+=16)
			{	int ly = ((y-8) >> 4) * wtmp;
				int by = ((int)(therm_off_y * hei) + (int)(therm_sz_y * hei * (y/16)));
				
				// printf("Processing line %i offset %i...\n", y, by);
				
				for (int x=8; x<128; x+=16)
				{	int bx = therm_off_x * wid + therm_sz_x * wid * (x/16);
					int lx = ly+(x>>4);
					char *rgb;
					
					// therm_mode == _THM_FIRE
					switch (therm_mode)
					{	case _THM_FIRE:
						rgb = fire_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_LAVA:
						rgb = lava_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_FEVR:
						rgb = rgb_from_temp_fever(reading);
						break;
						
						case _THM_ROOM:
			            rgb = centered_grad(reading, 5*4+25, 40*4+25, 25*4+25);
			            break;
			            
			            case _THM_AMB:
			            rgb = centered_grad(reading, tempmin-5, tempmax+5, thermdat[1023]);
			            break;
						
						case _THM_BLEU:
						case _THM_TIV:
						rgb = bleu_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_HUE:
						default:
						rgb = rgb_from_temp(reading);
						break;
					}
					
					int tsi = ((x-8)>>4)+_THERM_W*((y-8)>>4);
					int bmi = bx + wid*by;
					thermspot[tsi].x = bx;
					thermspot[tsi].y = by;
					thermspot[tsi].r = rdat[bmi];
					thermspot[tsi].g = gdat[bmi];
					thermspot[tsi].b = bdat[bmi];
					
					thermr[tsi] = rgb[0];
					thermg[tsi] = rgb[1];
					thermb[tsi] = rgb[2];
					
#endif
					
#ifdef _MLX90640
					int reading = thermdat[lx] & 0xffff;
					if (reading & 0x8000) reading -= 0x10000;
					reading = 0.4*reading + 160;
					reading *= thmult[lx];
#else
					int reading = thermdat[lx] & 0xfff;
					if (reading & 0x800) reading -= 0x1000;
#endif
					
					// printf("Processing pixel %i bx %i...\n", x, bx);
					
					
					if (!thermfill)
					{
						for (int sqy = by - sq_hsz; sqy < by + sq_hsz; sqy++)
						{	if (sqy < 0 || sqy >= maxby) continue;
							int by2 = sqy * wid;
							for (int sqx = bx - sq_hsz; sqx < bx + sq_hsz; sqx++)
							{	if (sqx < 0 || sqx >= wid) continue;
								int bx2 = by2+sqx;
								
								rdat[bx2] = rgb[0];
								gdat[bx2] = rgb[1];
								bdat[bx2] = rgb[2];
							}
						}
					
					}
					else
					{		;
					}					
					
				}
			}
			
			_exit_thermxy:
			;
			
			
			if (thermfill)
			{	for (int sqy = thermspot[0].y - hei*0.5*therm_sz_y; sqy < thermspot[767-80].y + hei*0.5*therm_sz_y; sqy++)
				{	if (sqy < 0 || sqy >= maxby) continue;
					int by2 = sqy * wid;
					for (int sqx = thermspot[0].x - wid*0.5*therm_sz_x; sqx < thermspot[767-1].x + wid*0.5*therm_sz_x; sqx++)
					{	if (sqx < 0 || sqx >= wid) continue;
						int bx2 = by2+sqx;
						
						pixel_5d p5;
						p5.x = sqx;
						p5.y = sqy;
						p5.r = rdat[bx2];
						p5.g = gdat[bx2];
						p5.b = bdat[bx2];
						
						
#if 0
                        int tsi = nearest_thermspot_5d(p5);
                        
                        if (therm_mode == _THM_TIV)
						{   bdat[bx2] = 0.5*bdat[bx2] + 0.5*gdat[bx2];
						    gdat[bx2] = rdat[bx2];
						    rdat[bx2] = thermr[tsi];
						}
						else
						{   float mxrgb = rdat[bx2];
						    if (gdat[bx2]> mxrgb) mxrgb = gdat[bx2];
						    if (bdat[bx2]> mxrgb) mxrgb = bdat[bx2];

						    mxrgb += _thermlite;
						    mxrgb /= (261 + _thermlite);	
						    if (mxrgb > 1) mxrgb = 1;
						    
						    rdat[bx2] = mxrgb * thermr[tsi];
						    gdat[bx2] = mxrgb * thermg[tsi];
						    bdat[bx2] = mxrgb * thermb[tsi];
						}
                        
#else
						
						wghidx w = blur_thermspot(p5);
						
						float wr=0, wg=0, wb=0;
						
						if (therm_mode == _THM_TIV)
						{   for (int i=0; i<4; i++)
						    {	wr += w.wt[i] * (0.4*thermr[w.idx[i]] + 0.6*thermg[w.idx[i]]);
						    }
						    
						    if (wr > 255) wr = 255;
						    
						    bdat[bx2] = 0.5*bdat[bx2] + 0.5*gdat[bx2];
						    gdat[bx2] = rdat[bx2];
						    rdat[bx2] = wr;
						}
						else
						{   for (int i=0; i<4; i++)
						    {	wr += w.wt[i] * thermr[w.idx[i]];
							    wg += w.wt[i] * thermg[w.idx[i]];
							    wb += w.wt[i] * thermb[w.idx[i]];
						    }
						    
						    float mxrgb;
						    /*
						    mxrgb = rdat[bx2];
						    if (gdat[bx2]> mxrgb) mxrgb = gdat[bx2];
						    if (bdat[bx2]> mxrgb) mxrgb = bdat[bx2];
						    */
						    
						    mxrgb = 0.33*rdat[bx2]
						          + 0.34*gdat[bx2]
						          + 0.33*bdat[bx2]
						          ;
						    
						    mxrgb += _thermlite;
						    mxrgb /= (261 + _thermlite);	
						    if (mxrgb > 1) mxrgb = 1;
						    
						    rdat[bx2] = mxrgb * wr;
						    gdat[bx2] = mxrgb * wg;
						    bdat[bx2] = mxrgb * wb;
						}
						
#endif
						
					}
				}
				
			}
			
			
			
			
		}
	}
	
	
	int status;
	status = 0;
	
	// Crop thermal image
	if (in_therm && thermdat)
	{
		unsigned char *rdat1, *gdat1, *bdat1;
		
		int cropx = therm_off_x * wid;
		int cropy = therm_off_y * hei;
		int cropw = (therm_sz_x)*(_THERM_W-2)*wid;
		int croph = (therm_sz_y)*(_THERM_H-3)*hei;
		
		int pixels = cropw * croph;
		rdat1 = (unsigned char *)malloc(pixels);
		gdat1 = (unsigned char *)malloc(pixels);
		bdat1 = (unsigned char *)malloc(pixels);
		
		for (int y=0; y<croph; y++)
		{	int y1 = y*cropw;
			int y2 = (y+cropy)*wid;
			for (int x=0; x<cropw; x++)
			{	int x1 = y1+x;
				int x2 = y2+x+cropx;
				rdat1[x1] = rdat[x2];
				gdat1[x1] = gdat[x2];
				bdat1[x1] = bdat[x2];
			}
		}
		
		rdat = rdat1; gdat = gdat1; bdat = bdat1;
		wid=cropw; hei=croph;
	}
	
	
	
	
	if (!strcmp(out_comp+strlen(out_comp)-4, ".bmp"))
		mk_bmp(wid, hei, 1, rdat, gdat, bdat, out_comp);
	else
	{
		// make a PNG output
		bitmap_t outimg;
		int x;
		int y;

		/* Create an image. */

		outimg.width = wid;
		outimg.height = hei;

		outimg.pixels = calloc (outimg.width * outimg.height, sizeof (pixel_t));

		if (! outimg.pixels) 
		{   printf("Failed to allocate output pixels.\n");
			return -1;
		}

	
		for (y = 0; y < outimg.height; y++) 
		{   for (x = 0; x < outimg.width; x++) 
		    {   pixel_t * pixel = pixel_at (& outimg, x, y);
		        pixel->red   = rdat[x+wid*y];
		        pixel->green = gdat[x+wid*y];
		        pixel->blue  = bdat[x+wid*y];
		    }
		}

		/* Write the image to a file 'outimg.png'. */

		if (save_png_to_file (& outimg, out_comp)) 
		{   fprintf (stderr, "Error writing %s.\n", out_comp);
			status = -1;
		}
		
		free(outimg.pixels);
	}
	
	
	if (del_inp_f)
	{   char cmdb[1024];
	    sprintf(cmdb, "sudo rm %s", in_rgi);
	    system(cmdb);
	}
	
	
	// create an output thumbnail
	char* otnfn = thumb_name(out_comp);
	
	printf("Thumbnail: %s\n", otnfn);
	
#define _THUMBWID 180
#define _THUMBHEI 120

	unsigned int rtn[_THUMBWID*_THUMBHEI],
	             gtn[_THUMBWID*_THUMBHEI],
	             btn[_THUMBWID*_THUMBHEI],
	             dvb[_THUMBWID*_THUMBHEI];
	float scx = (float)_THUMBWID/wid;
	float scy = (float)_THUMBHEI/hei;
	
	int twid = _THUMBWID, thei = _THUMBHEI;
	
	for (int i=0; i<_THUMBWID*_THUMBHEI; i++) 
	    dvb[i] = rtn[i] = gtn[i] = btn[i] = 0;
	
	for (int y=0; y<hei; y++)
	{   int y1 = y*wid;
	    int dy = y * scy;
	    int dy1 = dy*twid;
	    
	    for (int x=0; x<wid; x++)
	    {   int x1 = x + y1;
	        int dx = x * scx;
	        int dx1 = dx + dy1;
	        
	        rtn[dx1] += rdat[x1];
	        gtn[dx1] += gdat[x1];
	        btn[dx1] += bdat[x1];
	        dvb[dx1]++;
	    }
    }
    
    
    unsigned char rtnc[_THUMBWID*_THUMBHEI],
                  gtnc[_THUMBWID*_THUMBHEI],
                  btnc[_THUMBWID*_THUMBHEI];
                  
    for (int i=0; i<_THUMBWID*_THUMBHEI; i++) 
    {   float invdvb = 1.0 / dvb[i];
        rtnc[i] = rtn[i] * invdvb;
        gtnc[i] = gtn[i] * invdvb;
        btnc[i] = btn[i] * invdvb;
    }
    
    mk_bmp(twid, thei, 1, rtnc, gtnc, btnc, otnfn);
    free(otnfn);
	
	if (!in_therm && cmapping == _rgi)
    {   unsigned char rhist[10000],
	                  ghist[10000],
	                  bhist[10000];
        int r, y, g, b;
        float hmult = histmx ? 1.0 / histmx : 1;
        
        for (int yb=0; yb<100; yb++)
        {   y = 5.11*(yb < 50 ?     yb : 50);
            b = 5.11*(yb > 50 ? 100-yb : 50);
            if (b > 255) b = 255; if (b < 0) b = 0;
            
            for (int rg=0; rg<100; rg++)
            {   r = y + 5.11*(rg-50);
                g = y - 5.11*(rg-50);
                            
                if (r > 255) r = 255; if (r < 0) r = 0;
                if (g > 255) g = 255; if (g < 0) g = 0;
                
                int ix = rg + 100*yb;
                float hmpix = pow(hmult * histogram[ix], 0.25);
                
                rhist[ix] = (int)(r*hmpix) & 0xff;
                ghist[ix] = (int)(g*hmpix) & 0xff;
                bhist[ix] = (int)(b*hmpix) & 0xff;
            }
        }
        
        char histfn[1024];
        strcpy(histfn, out_comp);
        sprintf(histfn+33, ".hist.bmp");
	    mk_bmp(100, 100, 1, rhist, ghist, bhist, histfn);
	}
	
	return status;
}










