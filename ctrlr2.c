#include <gtk/gtk.h>
#include <gmodule.h>
#include <time.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <pthread.h>
#include "dispftns.h"
#include "globcss.h"
#include "mkbmp.h"

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

// device pixels
// #define SCR_RES_X 480
// #define SCR_RES_Y 320

// screen pixels
#define SCR_RES_X 720
#define SCR_RES_Y 480

#define THERM_RES_X 32
#define THERM_RES_Y 24

// Thermcam offset to align to NoIR - multiply these by wid, hei
float therm_off_x =     0.215;
float therm_off_y =     0.15;

// Thermcam is mounted crooked because I'm under the weather and drunk
float therm_rot_rad =   2.0 * 3.1415926535897932384626 / 180;

// NoIR size per thermcam 16x16-pixel block - multiply these by wid, hei
#define therm_sz_x 0.0223
#define therm_sz_y 0.0285

// Do not wait for images to process; use linked list
#define _SIMULSHOT 1

// pthread_t threads[256];
// int thread_is_active[256];

typedef struct fname4widget
{   GtkWidget *widg;
    char fname[256];
    int resize;
    GdkPixbuf** dest_pixbuf;
} fname4widget;


int *thermdat;
long *tmdat;
cairo_t *gcr;
cairo_t *prevcr;
cairo_t *prevhcr;
GtkWidget *gwidget;
GtkWidget *gwidgetp;
GtkWidget *gwidgetph;
GtkWidget *iplbl;
GdkPixbuf *gpixp, *gpixph, *gpixsv;
int cam_pid;
int flashir, flashv;
int firlit, fvlit;
int pics_taken = 0;

int ch_mapping, therm_pal;
int shutter, camres, expmode;
int flashy;
int cpstage=0, cam_on, vid_on=0;
int thermiter = 0;
int flashd = 0;
int tmlaps = 0;

char thermf[256];

int devbox = 0;
int allowvid = 0;

int imp, impt;

char ltfn[256], ltfnh[256];

#define _MAX_NOIRTYPE 6

char* ntdisp[_MAX_NOIRTYPE+1] = 
    {   "rgi", 
        "cir", 
        "mono", 
        "veg", 
        "redsky",
        "xmas",
        "bluwht"
    };
// char* ntdisph[_MAX_NOIRTYPE+1] = {"rgih", "cirh", "monh", "vegh", "grih"};
char* ntdisph[_THM_MAX+1] = 
    {   "fire", 
        "rnbo", 
        "fevr", 
        "room", 
        "amb",
        "hue",
        "bleu",
        "tiv",
        "lava"
    };


char* xposrmod[9] = { "snow",
                      "nightpreview",
                      "backlight",
                      "spotlight",
                      "sports",
                      "beach",
                      "verylong",
                      "antishake",
                      "fireworks"
                     };
char* xposrmdsh[9] = { "day",
                       "night",
                       "backlt",
                       "spotlt",
                       "sport",
                       "beach",
                       "vlong",
                       "noshake",
                       "firewrx"
                      };

/* Surface to store thermcam img */
static cairo_surface_t *surface = NULL;
static cairo_surface_t *surfacep = NULL;
static cairo_surface_t *surfaceph = NULL;
static cairo_surface_t *surfacesv = NULL;

GtkApplication *app;

GtkWidget *window;
GtkWidget *frame;
GtkWidget *drawing_area;
GtkWidget* preview_area;
GtkWidget* previewh_area;
GtkWidget *grid;

GtkWidget *slideshow;
GtkWidget *slsh_grid;
GtkWidget *slsh_lbut;
GtkWidget *slsh_rbut;
GtkWidget *slsh_view;
GtkWidget *slsh_xbut;
GtkWidget *slsh_dbut;
GtkWidget *slsh_flbl;
int slsh_lsidx;
char slsh_cimg[1024];
char delcut[11];

GtkWidget *menu;
GtkWidget *menu_grid;
GtkWidget *menu_btnrgi;
GtkWidget *menu_btncir;
GtkWidget *menu_btnmon;
GtkWidget *menu_btnveg;
GtkWidget *menu_btnrs;
GtkWidget *menu_btnxms;
GtkWidget *menu_btnib;

GtkWidget *menu_fire;
GtkWidget *menu_rnbo;
GtkWidget *menu_fevr;
GtkWidget *menu_room;
GtkWidget *menu_amb;
GtkWidget *menu_hue;
GtkWidget *menu_bleu;
GtkWidget *menu_tiv;

GtkWidget* reslbl;
GtkWidget* shutlbl;
GtkWidget* explbl;
GtkWidget* noirbtn;
GtkWidget* thbtn;
GtkWidget* xbtn;
GtkWidget* prevgrid;
GtkWidget* resgrid;
GtkWidget* resmbtn;
GtkWidget* respbtn;
GtkWidget* shexpgrid;
GtkWidget* shutgrid;
GtkWidget* shutmbtn;
GtkWidget* shutpbtn;                      // yep there's a shut up button
GtkWidget* flonbtn;
GtkWidget* recbtn;

GtkWidget* ifbtn;
GtkWidget* vfbtn;

int lxbx, lxby;

int have_ip = 0;
int had_ip = 1;             // assume 1 until you know for sure 0
int nopitft = 0;
int trupl = 0;

float thmult[780];         // plenty of RAM so avoid seg faults
int thsamp;
long double last_cam_init;
unsigned long last_flash;
int lxoff, pxoff, phxoff, shxoff;           // No not Phoenix off! Although it does get hot here.
int pcntused;

int ctdn2camreinit;
int pwrr;

typedef struct llistelem
{
    struct llistelem *listprev, *listnext;
    char* fname;
    int panelidx;
}
llistelem;

llistelem *listfirst;
unsigned int listlen;

void list_add(char* filename, int pnlidx)
{   // create new llistelem
    llistelem *nle = (llistelem*)malloc(sizeof(llistelem));
    nle->fname = (char*)malloc(strlen(filename)+2);
    strcpy(nle->fname, filename);
    nle->panelidx = pnlidx;
    nle->listnext = 0;
    nle->listprev = 0;
    
    // find last item
    if (!listlen) listfirst = 0;
    if (!listfirst) listfirst = nle;
    else
    {   llistelem *listlast = listfirst;
        while (listlast->listnext) listlast = listlast->listnext;
    
        // link new item and last item together
        nle->listprev = listlast;
        listlast->listnext = nle;
    }
    
    listlen++;
    
    print_list();
}


llistelem* list_remove(llistelem *toberemoved)
{   if (!toberemoved || !listlen) return 0;

    // if !listprev, repoint listfirst
    if (!toberemoved->listprev) listfirst = toberemoved->listnext;
    
    // else repoint listprev
    else toberemoved->listprev->listnext = toberemoved->listnext;
    
    // repoint listnext
    if (toberemoved->listnext) 
        toberemoved->listnext->listprev = toberemoved->listprev;
    
    llistelem* lnxt = toberemoved->listnext;
    
    // deallocate
    if (toberemoved->fname) 	free(toberemoved->fname);
    if (toberemoved)		free(toberemoved);
    
    listlen--;
    if (!listlen) listfirst = 0;
    
    print_list();
    
    return lnxt;
}

void print_list()
{
    if (!listlen) listfirst = 0;
    if (!listfirst) printf("\n\nEmpty list.\n\n");
    else
    {   llistelem *listlast = listfirst;
        while (listlast->listnext) 
        {   printf("List item %s panel %i\n",
                   listlast->fname,
                   listlast->panelidx
                  );
            listlast = listlast->listnext;
        };
    }
}

int
window_key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer user_data);


/******************************************************************************/
/* CONVERSION FUNCTIONS                                                       */
/******************************************************************************/

char** array_from_spaces(const char* input, int max_results)
{   char** result;

    // scan the input array until not space
    int i,j,k,n;
    
    k=0;
    result = (char*)malloc(max_results * sizeof(char*));
    for (i=0; input[i] == 32; i++);
    
    for (; input[i]; i++)
    {
        // if zero, exit
        if (!input[i]) break;
        
        // printf("%d %c\n", i, input[i]);
        
        // scan forward until space or zero
        for (j=i+1; input[j] && input[j] != 32; j++) ;
        
        // copy range
        int l = j - i;
        result[k] = malloc(l+2);
        for (n=0; n<l; n++)
        {   result[k][n] = input[i+n];
        }
        result[k][n] = 0;
        
        printf("%s\n", result[k]);
        
        k++;
    
        // resume at end of range
        if (!input[j] || k >= max_results)
        {   result[k] = malloc(2);
            result[k][0] = 0;
            break;
        }
        
        for (i=j+1; input[i] == 32; i++);
        i--;
    }
    
    return result;
}

static int strsort_cmp(const void* a, const void* b) 
{ 
   return strcmp(*(const char**)a, *(const char**)b); 
} 

void strsort(const char* arr[], int n) 
{  qsort(arr, n, sizeof(const char*), strsort_cmp); 
} 


/******************************************************************************/
/* SYSTEM HOUSEKEEPING FUCTIONS                                               */
/******************************************************************************/

long update_tmdat(int gimmeaminute)
{   long newval = time(NULL) + gimmeaminute;
    
    if (newval > tmdat[2047]) tmdat[2047] = newval;
    tmdat[2046] = getpid();
}

int is_process_running(char* pname)
{   char cmd[1024];
    // sprintf(cmd, "ps -ef | grep %s | grep -v %i", pname, getpid());
    sprintf(cmd, "ps -ef | grep %s | grep -v grep", pname);
    
    FILE* pf = popen(cmd, "r");
    if (pf)
    {   int lines = 0;
        char buffer[1024];
        while (fgets(buffer, 1024, pf))
        {   /*if (!strstr(buffer, "grep "))*/ lines++;
        }
        fclose(pf);
        return lines;
    }
    return -1;
}

int get_process_pid(pname)
{   char cmd[1024];
    sprintf(cmd, "ps -ef | grep %s", pname);
    FILE* pf = popen(cmd, "r");
    if (pf)
    {   int lines = 0;
        char buffer[1024];
        while (fgets(buffer, 1024, pf))
        {   if (!strstr(buffer, "grep "))
            {   buffer[14] = 0;
                return atoi(buffer+9);
            }
        }
        fclose(pf);
        return lines;
    }
    return 0;
}

void log_action(char* logmsg)
{   if (!devbox) return;

    FILE* pf = fopen("/home/pi/act.log", "a");
    if (pf)
    {   char buffer[1024];
    
        FILE* pfd;
        pfd = popen("date", "r");
        if (pfd)
        {   fgets(buffer, 1024, pfd);
            fclose(pfd);
        }   else strcpy(buffer, "date failed");     // story of my life
        
        fprintf(pf, "%s: %s\n", buffer, logmsg);
        fclose(pf);
    }
}

/*
 * Check if a file exist using fopen() function
 * return 1 if the file exist otherwise return 0
 * WARNING: VERY KLUDGY! I'm not crediting the author so they can save face.
 */
int cfileexists(const char * filename)
{   /* try to open file to read */
    FILE *file;
    if (file = fopen(filename, "r"))
    {   fclose(file);
        return 1;
    }
    return 0;
}


void check_disk_usage()
{   FILE* fp;
    char buffer[1024];
        
    if (fp = popen("df | grep /dev/root", "r"))
    {   fgets(buffer, 1024, fp);
        printf("%s", buffer);
        char** arr = array_from_spaces(buffer, 7);
        // arr[4][strlen(arr[4]-1)] = 0;
        pcntused = atoi(arr[4]);
        fclose(fp);
    }
}

static gboolean force_gdbkp()
{
    if (!have_ip) return FALSE;
    
    if (!trupl)
    {   // Initiate cloud backup and set flag, then set future checks.
        system("/bin/bash /home/pi/gdbkp.sh &");
        trupl = 1;
        return TRUE;
    }   else
    {   // Once cloud backup finishes, clear flag and end checks.
        if (is_process_running("gdbkp")) return TRUE;
        else
        {   trupl = 0;
            return FALSE;
        }
    }
    
    return FALSE;
}


void raspicam_cmd_format(char* cmd, int signal, int is_vid)
{   char shut[64], *res;
    int lshut;
    
    lshut = shutter;
    if (signal && lshut > 3000000) lshut = 3000000;
    if (lshut > 0)
    {   sprintf(shut, "--shutter %i", lshut);
    }
    else strcpy(shut, "");
    
    switch (camres)
    {   
        case 480:
        res = "-w 640 -h 480";
        break;
        
        case 768:
        res = "-w 1024 -h 768";
        break;
        
        case 1080:
        res = "-w 1920 -h 1080";
        break;
        
        case 600:
        default:
        res = "-w 800 -h 600";
    }
    
    long double t = time(NULL);
    if ((t - last_cam_init) < 2)
    {   // reinit is fucking up again
        // system("/bin/bash /home/pi/reinit_fuctup.sh");
        // system("sudo reboot");
        
        return;
    }
    
    last_cam_init = t;
    
    update_tmdat(15+(atoi(shut)/100000));
    
    char* cmdexc = is_vid 
                 // ? "raspivid -t 999999" 
                 ? "raspivid -t 0 -fps 24 -b 5000000" 
                 : "raspistill"
                 ;
    
    char* outfn = is_vid
                ? "/tmp/output.h264"
                // ? "- | ffmpeg -i - -f alsa -ac 1 -i hw:0,0 -map 0:0 -map 1:0 -vcodec copy -acodec aac -strict -2 /tmp/output.flv"
                : "/tmp/output.jpg"
                ;
    
    raspistill_end_misery(signal 
                        ? (is_vid ? "video" : "reinit" )
                        : "long exposure"
                         );
    sprintf(cmd, "%s %s -p '192,120,360,240' -op 160 -sa 50 %s %s -vf -hf -awb greyworld -ex %s --drc high -o %s %s"
                , cmdexc
                , signal?"--signal":""
                , res
                , shut
                , xposrmod[expmode]
                , outfn
                , (signal|is_vid)?"&":""
           );
    log_action(cmd);
}



void raspistill_init()
{   char cmd[1024];

    if (cpstage || !cam_on || vid_on) return;
    
    raspicam_cmd_format(cmd, 1, 0);
    system(cmd);
    if (shutter > 0) usleep(shutter*6.5);
    else usleep(100000);
    cam_pid = vid_on
                ? get_process_pid("raspivid")
                : get_process_pid("raspistill");
    // printf("Camera PID is %i my PID is %i\n", cam_pid, getpid());
    
    sprintf(cmd, "sudo renice -n 7 -p %i", cam_pid);
    system(cmd);
    
    if (shutter > 427503)
    {   system("sudo pkill fbcp");
        usleep(200000);
        if (!is_process_running("fbcp")) system("fbcp &");
    }
}

void raspistill_end_misery(char* reason)
{   char buffer[1024];
    sprintf(buffer, "pkill raspistill for reason: %s", reason);
    log_action(buffer);
    system("sudo pkill raspistill");
    
    if (shutter > 427503)
    {   system("sudo pkill fbcp");
        usleep(200000);
        if (!is_process_running("fbcp")) system("fbcp &");
    }
}

// https://stackoverflow.com/questions/6898337/determine-programmatically-if-a-program-is-running
pid_t proc_find(const char* name) 
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) 
    {   log_action("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) 
    {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') 
        {   continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) 
        {   if (fgets(buf, sizeof(buf), fp) != NULL) 
            {   /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (!strcmp(first, name)) 
                {   fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return 0;
}


int check_processes()
{   char buffer[1024];

    update_tmdat(0);
    
    if (!is_process_running("fbcp"))
    {   nopitft++;
        if (nopitft > 5)
        {   printf("Starting PiTFT...\n");
            system("fbcp &");
            nopitft = 0;
        }
    }
    else nopitft = 0;
    
    if (!is_process_running("thermcam2"))
    {   printf("Starting therm...\n");
        system("/home/pi/thermcam2 123 -nop > /dev/null &");
    }
    
    DIR* dir = NULL;
    sprintf(buffer, "/proc/%i", cam_pid);
    if (!cam_pid || !(dir = opendir(buffer))) 
    {
        if (cam_on < 0)
        {   cam_on++;
            return 1;
        }
    
        // int result = is_process_running("raspistill");
        int result = proc_find("raspistill");
        if (!result)
        {   system("ps -ef | grep raspistill >> /home/pi/act.log");
            sprintf(buffer, "Process check %i starting raspistill...\n", result);
            log_action(buffer);
            raspistill_init();
            usleep(200000);
            upd_lbl_shut();
        }
        cam_pid = vid_on
                ? get_process_pid("raspivid")
                : get_process_pid("raspistill");
        // printf("Camera PID is %i my PID is %i\n", cam_pid, getpid());
    }
    if (dir) closedir(dir);
    
    return 1;
}

void load_thermal_mult(void)
{   int i;
    
    for (i=0; i<768; i++) thmult[i] = 1;
    
    if (thsamp < 0)             // load presets
    {   FILE* pf = fopen("/home/pi/thmult.dat", "r");
        if (pf)
        {   char buffer[1024];
            for (i=0; i<768; i++)
            {   fgets(buffer, 1024, pf);
                thmult[i] = atof(buffer);
                if ( !thmult[i] ) thmult[i] = 1;
            }
            fclose(pf);
        }
    }
    else                        // preset generation mode
    {   ;
    }
}


/******************************************************************************/
/* BASIC GRAPHICS STUFF                                                       */
/******************************************************************************/

static void
clear_surface (void)
{
  cairo_t *cr;

  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 2, SCR_RES_Y / 2);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfacep (void)
{
  cairo_t *cr;

  cr = cairo_create (surfacep);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 4, SCR_RES_Y / 4);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfaceph (void)
{
  cairo_t *cr;

  cr = cairo_create (surfaceph);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 4, SCR_RES_Y / 4);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfacev (void)
{
  cairo_t *cr;

  cr = cairo_create (surfacesv);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
}

GdkPixbuf * draw_image_widget(GtkWidget *da, char* fname, int resz)
{
    GError *err = NULL;
    GdkPixbuf *pix;
    /* Create pixbuf */
    pix = gdk_pixbuf_new_from_file(fname, &err);
    if(err)
    {
        printf("Error : %s\n", err->message);
        g_error_free(err);
        return FALSE;
    }
    
    if (!pix) printf("ERROR pix is null!");
    
    gint w_i, h_i;
    gdk_pixbuf_get_file_info(fname, &w_i, &h_i);
    
    int w, h, x=0;
    gtk_widget_get_size_request(da, &w, &h);
    
    float inrat, outrat;
    inrat  = (float)w_i / h_i;
    outrat = (float)w   / h  ;
    
    if (outrat > inrat) 
    {   x = w;
        w = h * inrat;
        x -= w;
        x /= 2;
    }
    
    if (resz)
    {
        printf("Scaling to %i, %i px, aspect ratio %f, with x offset %i\n", w, h, inrat, x);

        GdkPixbuf *scaled;
        scaled = gdk_pixbuf_scale_simple(pix,
                                         w,
                                         h,
                                         GDK_INTERP_BILINEAR
                                        );
        if (!scaled) printf("ERROR scaled is null!");
        
        
        GdkWindow *draw = gtk_widget_get_window(da);
        
        cairo_t *cr = gdk_cairo_create (draw);
        gdk_cairo_set_source_pixbuf (cr, scaled, x, 0);

        lxoff = x;
        cairo_paint (cr);
        cairo_destroy (cr);
        
        gtk_widget_queue_draw(da);
        
        return scaled;
    }   else
    {
        GdkWindow *draw = gtk_widget_get_window(da);
        
        cairo_t *cr = gdk_cairo_create (draw);
        gdk_cairo_set_source_pixbuf (cr, pix, x, 0);

        lxoff = x;
        cairo_paint (cr);
        cairo_destroy (cr);
        
        gtk_widget_queue_draw(da);
        
        return pix;
    }
}

void img_widg_thread(fname4widget *fn4w)
{   // pthread_t

    printf("Thread drawing image %s on widget %8x...\n",
            fn4w->fname,
            fn4w->widg
          );
          
    // while (!cfileexists(

    GdkPixbuf *pb =
    draw_image_widget(fn4w->widg,
                      fn4w->fname,
                      fn4w->resize
                     );
    
    if (fn4w->dest_pixbuf) *(fn4w->dest_pixbuf) = pb;
    
    usleep(100000);
    
    if (fn4w) free(fn4w);
    printf("Exiting thread.\n");
    pthread_exit(NULL);
}



/******************************************************************************/
/* UI UPDATERS                                                                */
/******************************************************************************/

void
offer_delete_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   char** cleanls = {"ls -1 /home/pi/Pictures/",
					  "ls -1 /home/pi/Videos/"
					 };
					 
    if (response_id == -8)          // yes
    {   FILE* fp;
    
    	for (int i=0; i<2; i++)
    	{	fp = popen(cleanls[i],"r");
		    char buffer[256], cmdbuf[256];
		    if (fp)
		    {   while (!feof(fp))
		        {   fgets(buffer, 256, fp);
		            if (buffer[8] == '.')
		            {   buffer[8] = 0;
		                if (strcmp(buffer, delcut) <= 0)
		                {   buffer[8] = '.';
		                    sprintf(cmdbuf, 
		                            "sudo rm /home/pi/Pictures/%s",
		                            buffer
		                           );
		                    system(cmdbuf);
		                }
		                else break;
		            }
		        }
		        fclose(fp);
		    }
        }
    }
    
    cam_on = 1;
    raspistill_init();
}

void offer_delete()
{   char buffer[256], buf1[256], buf2[256];
    
    strcpy(buf1, "00000000");
    strcpy(buf2, "1970 Jan 01");
    FILE* fp = popen("/bin/bash /home/pi/medianpic.sh", "r");
    if (fp)
    {   fgets(buf1, 256, fp);
        fgets(buf2, 256, fp);
        fclose(fp);
    }
    
    for (int i=0; i<256; i++)
    {   if (buf1[i] < 32)
        {   buf1[i] = 0;
            break;
        }
    }
    
    strcpy(delcut, buf1);
    
    if (pcntused < 99)
    {   sprintf(buffer, 
                "Memory is %i%s full. \nDelete photos from\n%sand older?", 
                pcntused, 
                "%%",
                buf2
               );
    }
    else
    {   sprintf(buffer, 
                "MEMORY FULL. \nDelete photos from\n%sand older?", 
                buf2
               );
    }
    
    cam_on = 0;
    raspistill_end_misery("Memory usage message");
    
    GtkWidget * dlg = gtk_message_dialog_new (window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        buffer
        );
        
    g_signal_connect(dlg, "response",
                     G_CALLBACK (offer_delete_response), 
                     NULL
                    );

    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}


void upd_btn_pwr()
{	GtkStyleContext *context;
    context = gtk_widget_get_style_context(xbtn);
    if (pwrr)
    {	gtk_style_context_remove_class(context, "pwr");      
    	gtk_style_context_add_class(context, "pwrr");
	}
	else
	{	gtk_style_context_remove_class(context, "pwrr");      
    	gtk_style_context_add_class(context, "pwr");
	}
}

void upd_lbl_shut()
{   
    switch (shutter)
    {   
        case 1000000:
        gtk_button_set_label(shutlbl, "Sh: 1s");
        break;
        
        case 2000000:
        gtk_button_set_label(shutlbl, "Sh: 2s");
        break;
        
        case 3500000:
        gtk_button_set_label(shutlbl, "Sh: 3.5s");
        break;
        
        case 5889950:
        gtk_button_set_label(shutlbl, "Sh: 6s");
        break;
        
        case 750000:
        gtk_button_set_label(shutlbl, "Sh: 3/4s");
        break;
        
        case 500000:
        gtk_button_set_label(shutlbl, "Sh: 1/2s");
        break;
        
        case 250000:
        gtk_button_set_label(shutlbl, "Sh: 1/4s");
        break;
        
        case 166667:
        gtk_button_set_label(shutlbl, "Sh: 1/6s");
        break;
        
        case 100000:
        gtk_button_set_label(shutlbl, "Sh: 1/10s");
        break;
        
        case 50000:
        gtk_button_set_label(shutlbl, "Sh: 1/20s");
        break;
        
        case 20000:
        gtk_button_set_label(shutlbl, "Sh: 1/50s");
        break;
        
        case 10000:
        gtk_button_set_label(shutlbl, "Sh: 10ms");
        break;
        
        case 3000:
        gtk_button_set_label(shutlbl, "Sh: 3ms");
        break;
        
        case 1000:
        gtk_button_set_label(shutlbl, "Sh: 1ms");
        break;
        
        case 333:
        gtk_button_set_label(shutlbl, "Sh: 333µs");
        break;
        
        case 100:
        gtk_button_set_label(shutlbl, "Sh: 100µs");
        break;
        
        case 33:
        gtk_button_set_label(shutlbl, "Sh: 33µs");
        break;
        
        case 10:
        gtk_button_set_label(shutlbl, "Sh: 10µs");
        break;
        
        case 3:
        gtk_button_set_label(shutlbl, "Sh: 3µs");
        break;
        
        default:
        gtk_button_set_label(shutlbl, "Sh: Auto");
    }
    
}


void upd_lbl_exp()
{   char buffer[16];
    sprintf(buffer, "Exp: %s", xposrmdsh[expmode]);
    gtk_button_set_label(explbl, buffer);   
}



void upd_lbl_res()
{
    switch (camres)
    {   
        case 480:
        gtk_label_set_text(reslbl, "640\nx480");
        break;
        
        case 768:
        gtk_label_set_text(reslbl, "1024\nx768");
        break;
        
        case 1080:
        gtk_label_set_text(reslbl, "1080p");
        break;
        
        case 600:
        default:
        gtk_label_set_text(reslbl, "800\nx600");
    }
    
}


int thermdraw()
{
    if (!thermdat) return 1;
    
    int x, y;
    cairo_t *lcr;
    
    float thx, thy, xoff, yoff;
    
    xoff = (therm_off_x-0.1)*SCR_RES_X / 2;
    yoff = (therm_off_y-0.1)*SCR_RES_Y / 2;
    
    thx = 1.2*therm_sz_x*SCR_RES_X / 2;
    thy = 1.2*therm_sz_y*SCR_RES_Y / 2;

    lcr = cairo_create (surface);
    
    int thmax, thmin;
    cairo_set_source_rgb (lcr, .602, .503, .427);
    cairo_rectangle(lcr, 0, 0, SCR_RES_X / 2, SCR_RES_Y / 2);
    cairo_fill(lcr);
    
    if (!thermiter && !vid_on && pcntused >= 80)
    {   offer_delete();
    }
    
    thermiter++;
    if (pwrr) 
    {	pwrr--;
    	if (!pwrr) upd_btn_pwr();
	}
    
    if (1) //therm_pal)
    {   for (x=0; x<THERM_RES_X*THERM_RES_Y; x++)
        {   if (!x || thermdat[x] > thmax) thmax = thermdat[x];
            if (!x || thermdat[x] < thmin) thmin = thermdat[x];
        }
    }
    
    thmax += 2;
    thmin -= 2;
    
    if (vid_on)
    {   GtkStyleContext *context = gtk_widget_get_style_context(recbtn);
        if (thermiter & 4) gtk_style_context_add_class(context, "redbk");
        else gtk_style_context_remove_class(context, "redbk");
    }
    
    unsigned char rdat[THERM_RES_X*THERM_RES_Y],
                  gdat[THERM_RES_X*THERM_RES_Y],
                  bdat[THERM_RES_X*THERM_RES_Y]
                  ;
    int dx, dy, dx1, dy1;
    for (y=0; y<THERM_RES_Y; y++)
    {   dy = /*SCR_RES_Y/4 +*/ yoff+y*thy;     
        dy1 = THERM_RES_X*y;
        for (x=0; x<THERM_RES_X; x++)
        {   dx = /*SCR_RES_X / 4 +*/ xoff+x*thx;
            dx1 = x+dy1;
            int reading = thermdat[dx1];
            
            if (thsamp >=0) thmult[dx1] += reading;
            else reading *= thmult[dx1];
            
            unsigned char *rgb;
								   
		    switch (therm_pal)
		    {	case _THM_FIRE:
			    rgb = fire_grad(reading, thmin, thmax);
			    break;
			    
			    case _THM_FEVR:
			    rgb = rgb_from_temp_fever(reading);
			    break;
			    
			    case _THM_ROOM:
			    rgb = centered_grad(reading, 5*4+25, 40*4+25, 25*4+25);
			    break;
			    
			    case _THM_AMB:
			    rgb = centered_grad(reading, thmin-5, thmax+5, thermdat[1023]);
			    break;
			    
			    case _THM_RAINB:
			    rgb = rgb_in_minmax(reading, thmin, thmax);
			    break;
			    
			    case _THM_BLEU:
			    rgb = bleu_grad(reading, thmin, thmax);
			    break;
			    
			    case _THM_TIV:
			    rgb = bleu_grad(reading, thmin, thmax);
			    rgb[0] = 0.4*rgb[0] + 0.6*rgb[1];
			    rgb[1] = rgb[2] = 0;
			    break;
			    
			    case _THM_HUE:
			    default:
			    rgb = rgb_from_temp(reading);
			    break;
			    
			    case _THM_LAVA:
			    rgb = lava_grad(reading, thmin, thmax);
			    break;
			    
			    
		    }
		    
		    rdat[dx1] = rgb[0];
		    gdat[dx1] = rgb[1];
		    bdat[dx1] = rgb[2];
            
            cairo_set_source_rgb(lcr, 0.0039*rgb[0], 0.0039*rgb[1], 0.0039*rgb[2]);
            cairo_rectangle(lcr, 
                            dx, 
                            dy, 
                            dx+thx, 
                            dy+thy
                            );
            cairo_fill (lcr);
            
            
	    
            
        }
    }
    
    if (vid_on)
    {   char vfnbuf[256];
        sprintf(vfnbuf,
                "/tmp/thv%06d.bmp",
                thermiter
               );
        mk_bmp(THERM_RES_X,
               THERM_RES_Y,
               16,
               rdat, gdat, bdat,
               vfnbuf
              );
    }
    
    if (thsamp >=0) thsamp++;
    
    if (thsamp >= 1000)
    {   FILE* pf = fopen("thmult.dat", "w");
        if (pf)
        {   int i;
            float thavg = 0;
            for (i=0; i<768; i++) 
            {   thmult[i] /= thsamp;
                thavg += thmult[i];
            }
            thavg /= 768;
            
            for (i=0; i<768; i++)
            {   
                thmult[i] = thavg / thmult[i];
                fprintf(pf, "%f\n", thmult[i]);
            }
            fclose(pf);
        }   else printf("FAILED to open thmult.dat for writing.");
        
        system("sudo pkill ctrlr");
    }
    
    
    
    dx += thx;
    dy += thy;
    
    
    cairo_set_source_rgb(lcr, .666, .503, .427);
    cairo_rectangle(lcr,  0, dy, SCR_RES_X, SCR_RES_Y);
    cairo_fill (lcr);
    
    cairo_rectangle(lcr, dx,  0, SCR_RES_X, SCR_RES_Y);
    cairo_fill (lcr);
    
    cairo_destroy (lcr);

    /* Now invalidate the affected region of the drawing area. */
    // gtk_widget_queue_draw_area (gwidget, 0, 0, THERM_RES_X*10, THERM_RES_Y*10);
    gtk_widget_queue_draw(gwidget);
    
    
    if (ctdn2camreinit)
    {   ctdn2camreinit--;
        if (!ctdn2camreinit)
        {   raspistill_init();
        }
    }
    
    
#if _SIMULSHOT

    int counter = listlen;
    if (listfirst)
    {   llistelem *le = listfirst;
        while (le && counter)
        {   if (!is_process_running("imgcomb") && cfileexists(le->fname))
            {   
                // while (is_process_running("imgcomb")) usleep(100000);
                
                printf("Applying image %s with panel id %i\n",
                       le->fname,
                       le->panelidx
                      );
                
                
                
                pthread_t ptt;
                fname4widget *f4w;
                
                f4w = (fname4widget*)malloc(sizeof(fname4widget));
                
                switch (le->panelidx)
                {   case 1:
                    impt--;
                    if (!impt)
                    {
                        f4w->widg = gwidgetph;
                        strcpy(f4w->fname, le->fname);
                        f4w->resize = 0;
                        f4w->dest_pixbuf = &gpixph;
                        
                        if ( pthread_create(&ptt, 
                                             NULL, 
                                             img_widg_thread, 
                                             f4w
                                            )
                           )
                        {   printf("FAILED TO CREATE THREAD\n");
                            gpixph = draw_image_widget(gwidgetph, 
                                                      le->fname, 
                                                      0
                                                     );
                        
                        }
                    }
                    
                    // phxoff = lxoff;
                    break;
                    
                    case 0:
                    imp--;
                    if (!imp)
                    {
                        // gpixp  = draw_image_widget(gwidgetp, le->fname, 0);
                        f4w->widg = gwidgetp;
                        strcpy(f4w->fname, le->fname);
                        f4w->resize = 0;
                        f4w->dest_pixbuf = &gpixp;
                        
                        if ( pthread_create(&ptt, 
                                             NULL, 
                                             img_widg_thread, 
                                             f4w
                                            )
                           )
                        {   printf("FAILED TO CREATE THREAD\n");
                            gpixp = draw_image_widget(gwidgetp, 
                                                      le->fname, 
                                                      0
                                                     );
                        
                        }
                        // pxoff = lxoff;
                    }
                    break;
                    
                    default:
                    ;
                }

		// if (f4w) free(f4w);
                
                le = list_remove(le);
                counter--;
            }   
            else 
            {   /* printf("Image not ready %s for panel id %i\n",
                       le->fname,
                       le->panelidx
                      );
                */
                
                le = le->listnext;
            }
            
            counter--;
        }
    }
    
    
#endif

    

    return 1;
}


void
delete_old_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   // printf("Response: %i\n", response_id);
    // system("sudo pkill raspistill");
    // kill(getpid(),SIGINT);
    
    gtk_window_iconify(dialog);
    
    if (response_id == -8)          // yes
    {   char cmdbuf[1024], buffer[1024], filedate[1024];
        
        FILE *pf, *pf1;
        
        pf1 = popen("date -d \"yesterday 00:00\" +\x25Y\x25m\x25\x64", "r");
        if (pf1)
        {   while (fgets(buffer, 1024, pf1))
            {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
                {   strcpy(filedate, buffer);
                }
            }
            fclose(pf1);
        }
        
        pf = popen("ls /home/pi/Pictures/*.png | sort", "r");
        if (pf)
        {   while (fgets(buffer, 1024, pf))
            {   if (strcmp(&buffer[18], filedate) < 0)
                {
                    sprintf(cmdbuf, "sudo rm %s", buffer);
                    // sprintf(cmdbuf, "mv %s /home/pi/Pictures/test/", buffer);
                    system(cmdbuf);
                    // printf("%s\n", cmdbuf);
                }
            }
            
            fclose(pf);
        }
    }
    
    cam_on = 1;
}


int redraw_btns()
{
    
	
	
    gtk_widget_queue_draw(noirbtn);
    gtk_widget_queue_draw(thbtn);
    gtk_widget_queue_draw(xbtn);
    
    
    FILE *pf;
    
    
    pf = popen("ls /media/pi/", "r");
    if (pf)
    {
        char buffer[1000];
        buffer[0] = 0;
        fgets(buffer, 1000, pf);
        fclose(pf);
    
        if (strlen(buffer))
        {   cpstage++;
            
            switch(cpstage)
            {   case 1:
                gtk_label_set_text(iplbl, "Copying files...");
                return 1;
                
                case 2:
                cam_on = 0;
                printf("Copying files to %s...\n", buffer);
                // system("sudo pkill raspistill");
                raspistill_end_misery("copying to USB device");
                system("/bin/bash /home/pi/usbcp.sh");
                
                while (is_process_running("usbcp.sh"))
                {   usleep(100000);
                }
                
                GtkWidget * dlg = gtk_message_dialog_new (slideshow,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_OK,
                    "Please remove the USB device."
                    );
                
                
                gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
                
                
                dlg = gtk_message_dialog_new (slideshow,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_YES_NO,
                    "Delete old photos\n(> 1 day)?"
                    );
                
                g_signal_connect(dlg, "response",
                                  G_CALLBACK (delete_old_response), NULL);

                gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
                
                
                return 1;
                
                case 3:
                gtk_label_set_text(iplbl, "Safe to rmv USB.");
                cpstage=0;
                break;
                
                default:
                ;
            }
        }
        else cpstage=0;
    }
    
    
    
    
    // IP address
    pf = popen("ifconfig | grep \"inet \" | grep \"cast \"", "r");
    if (pf)
    {
        char buffer[1000];
        int i, j;
        // for (i=0; i<256; i++) buffer[i] = 0;
        
        fgets(buffer, 1000, pf);
        fclose(pf);
        
        int found=0;
        if (strlen(buffer)>6) // && strchr(buffer, '.'))
        {   for (i=1; i<256; i++)
            {   
                if (buffer[i] >= '0' && buffer[i] <= '9'
                   ) 
                {   found++;
                    break;
                }
            }
            
            
            
            if (found)
            {
                for (j=i; j<i+16; j++)
                {   if (buffer[j] != '.'
                        && (buffer[j] < '0' || buffer[j] > '9')
                       )
                    {   buffer[j] = 0;
                        break;
                    }
                }
            }
            
            buffer[i+16] = 0;
        }
                
        time_t rt;
        struct tm *lt;
        time(&rt);
        lt = localtime(&rt);
        
        if (found)
        {   if (!strchr(&buffer[i], '.')) found = 0;
            if (strlen(&buffer[i]) < 7) found = 0;
        }
        
        char buffer2[1024];
        char thcbuf[7];
        
        if (thsamp >= 0)
        {   strcpy(thcbuf, "thc ");
        }
        else thcbuf[0] = 0;
        
        if (found)
        {   had_ip = have_ip;
            have_ip = 1;
            
            // if (!had_ip && have_ip && !is_process_running("gdbkp")) system("/bin/bash /home/pi/gdbkp.sh &");
            
        	if (is_process_running("gdbkp"))
            {   /*int j = strlen();
                strcpy(&buffer[i+j], " ^");*/
                sprintf(buffer2, 
                        "%s%s%02i:%02i:%02i %s %i%%^", 
                        devbox ? "$" : "",
                        thcbuf,
                        lt->tm_hour,
                        lt->tm_min,
                        lt->tm_sec,
                        &buffer[i],
                        pcntused
                       );
            }   else
            {   sprintf(buffer2, 
                        "%s%s%02i:%02i:%02i %s %i%%", 
                        devbox ? "$" : "",
                        thcbuf,
                        lt->tm_hour,
                        lt->tm_min,
                        lt->tm_sec,
                        &buffer[i],
                        pcntused
                       );
            }
            // printf("%s\n", buffer2);
            gtk_label_set_text(iplbl, buffer2);
        }
        else
        {   
            sprintf(buffer2, 
                    "%s%s%02i:%02i:%02i %s %i%%", 
                    devbox ? "$" : "",
                    thcbuf,
                    lt->tm_hour,
                    lt->tm_min,
                    lt->tm_sec,
                    "No Internet",
                    pcntused
                   );
            gtk_label_set_text(iplbl, buffer2);
        }
    }
    
    return 1;
}


void slsh_view_oldest()
{   FILE* pf = popen("ls /home/pi/Pictures/*.png | sort -r", "r");
    if (pf)
    {   char buffer[512];
        int i=0;
        
        while (fgets(buffer, 512, pf))
        {   i++;
        }
        
        slsh_lsidx = i-1;
        slsh_view_file();
        
        fclose(pf);
    }
}


void slsh_view_file()
{   printf("Opening ls for images.\n");

	FILE* pf;
	char buffer[512];
	int dirlen;
    
	pf = popen("ls /home/pi/Pictures | wc -l | sed 's/[^0-9]*//g'", "r");
	if (pf)
	{	fgets(buffer, 512, pf);
		fclose(pf);
		dirlen = atoi(buffer);
		if (slsh_lsidx >= dirlen) slsh_lsidx = 0;
	}

    pf = popen("ls /home/pi/Pictures/*.png | sort -r", "r");
    if (pf)
    {   int i;
        
        strcpy(buffer, "");
                
        printf("Checking file existence 1/2.\n");
        if (ltfn[0] > 32)
        {   if (!cfileexists(ltfn)) 
            {   strcpy(ltfn, "");
            }
        }
                
        printf("Checking file existence 2/2.\n");
        if (ltfnh[0] > 32)
        {   if (!cfileexists(ltfnh)) 
            {   strcpy(ltfnh, "");
            }
        }
        
        printf("Checking file index.\n");
        if (slsh_lsidx < 0 && !ltfn[0]) slsh_lsidx = -1-slsh_lsidx;

        
        /*
        printf("ltfn = %s\n", ltfn);
        if (slsh_lsidx == -1)
        {   strcpy(slsh_cimg, ltfn);
            goto _have_fn_already;
        }
        if (slsh_lsidx == -2)
        {   strcpy(slsh_cimg, ltfnh);
            goto _have_fn_already;
        }
        */
        
        printf("slsh_lsidx = %i\n", slsh_lsidx);
        
        char very1st[1024];
        printf("Iterating results.\n");
        for (i=0; slsh_lsidx<0 || i<=slsh_lsidx; i++)
        {   fgets(buffer, 512, pf);
            buffer[strlen(buffer)-1] = 0;
            
            if (!i) strcpy(very1st, buffer);
            printf("lsidx %i is %s\n", i, buffer);
            
            if (slsh_lsidx == -1 && !strcmp(ltfn, buffer)) break;
            if (slsh_lsidx == -2 && !strcmp(ltfnh, buffer)) break;
            
            if (feof(pf))
            {   strcpy(buffer, very1st);
                slsh_lsidx = 0;
                break;
            }
        }
        
        if (slsh_lsidx >= 0)
        {   // fgets(buffer, 512, pf);
        }
        else slsh_lsidx = i;
        
        printf("Nulling empty buffer.\n");
        if (strlen(buffer) && buffer[strlen(buffer)-1] < 32) buffer[strlen(buffer)-1] = 0;
        
        printf("Storing image filename.\n");
        strcpy(slsh_cimg, buffer);
        
_have_fn_already:
        printf("Displaying image %s\n", slsh_cimg);
        if(!slsh_view) printf("ERROR slsh_view is null\n");
        printf("Drawing image widget %s.\n", slsh_cimg);
        gpixsv = draw_image_widget(slsh_view, slsh_cimg, 1);
        gtk_label_set_text(slsh_flbl, slsh_cimg+18);
        shxoff = lxoff;
        if (!gpixsv) printf("ERROR gpixsv is null\n");
        printf("Queueing image widget draw %s.\n", slsh_cimg);
        usleep(42778);
        gtk_widget_queue_draw(slsh_view);
        printf("Queued image widget draw %s.\n", slsh_cimg);
    }
}




/******************************************************************************/
/* CAMERA FUCTIONALITY                                                        */
/******************************************************************************/


static gboolean
flash_II(void)
{   cairo_t *cr;
    char filename[1024];
    char buffer[1024];
    char tmpfn[256];
    cr = cairo_create (surface);
    
    // flashy = 3;                    // wait on impl this
    
    // usleep(500000);
    
    while (!cfileexists("/tmp/output.jpg"))
    {  usleep(100000); 
    }
    
    system("gpio write 4 1");
    system("gpio write 0 1");
    firlit = fvlit = 0;
    
    FILE* pf = popen("date +\x25N", "r");
    if (pf)
    {   while (fgets(buffer, 1024, pf))
        {   if (buffer[0] >= '0' && buffer[0] <= '9')     
            {
                sprintf(tmpfn, "tmp%i.jpg", atoi(buffer));
                break;
            }
        }
        fclose(pf);
    }
    
    
    sprintf(buffer, "sudo mv /tmp/output.jpg /tmp/%s", tmpfn);
    system(buffer);
        
  
    // pf = popen("date +\x25Y\x25m\x25\x64.\x25H\x25M\x25S.\x25N", "r");
    pf = popen("date +\x25Y\x25m\x25\x64.\x25H\x25M\x25S", "r");
    if (pf)
    {   int lines = 0;
        while (fgets(buffer, 1024, pf))
        {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
            {   strcpy(filename, buffer);
                int i;
                for (i=0; i<1023; i++)
                {   if (filename[i] <= ' ') 
                    {   filename[i] = 0;
                        i += 1024;
                    }
                }
                sprintf(buffer, "Timestamp: %s\n", filename);
                // log_action(buffer);
                goto _thing;
                break;
            }
        }
        _thing:
        ;
        fclose(pf);
    }
  
  /*
  cairo_set_source_rgb (cr, 1, 1, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  usleep(200000);
  
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  usleep(200000);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  */
  // usleep(100000);
  
  if (filename)
  { char cmdbuf[1024], *ampersand;
    char fn[256], fnh[256];
    
    sprintf(fn, 
            "/home/pi/Pictures/%s.%s.png",
            filename,
            ntdisp[ch_mapping] // ? "cir" : "rgi"
           );
    
    sprintf(fnh, 
            "/home/pi/Pictures/%s.%s.png",
            filename,
            // (therm_pal == _THM_TIV) ? "tiv" : (ntdisph[ch_mapping])
            ntdisph[therm_pal]
           );
           
    char* fnth = thumb_name(fn);
    char* fnhth = thumb_name(fnh);
    
    strcpy(ltfn, fn);
    strcpy(ltfnh, fnh);
    
    FILE* fnfp = fopen("fnfn.dat", "w");
    if (fnfp)
    {   fprintf(fnfp, "%s\n%s\n", fnth, fnhth);
        fclose(fnfp);
    }
    
#if _SIMULSHOT
    list_add(fnth, 0);
    list_add(fnhth, 1);
#else        
    // strcpy(ltfn, fn);
    // strcpy(ltfnh, fnh);
#endif
   
#if _SIMULSHOT
    ampersand = "&";
#else
    // If taking 1080p, wait for processing before allow more pictures
    ampersand = "";
#endif
    
    char thparam[16];
    
    switch (therm_pal)
    {	case _THM_FIRE:
	    strcpy(thparam, "-fire");
	    break;
	    
	    case _THM_FEVR:
	    strcpy(thparam, "-fevr");
	    break;
	    
	    case _THM_ROOM:
	    strcpy(thparam, "-room");
	    break;
	    
	    case _THM_AMB:
	    strcpy(thparam, "-amb");
	    break;
	    
	    case _THM_RAINB:
	    strcpy(thparam, "-rain");
	    break;
	    
	    case _THM_BLEU:
	    strcpy(thparam, "-bleu");
	    break;
	    
	    case _THM_TIV:
	    strcpy(thparam, "-tiv");
	    break;
	    
	    case _THM_HUE:
	    default:
	    strcpy(thparam, "");
	    break;
	    
	    case _THM_LAVA:
	    strcpy(thparam, "-lava");
	    break;
    }
    
    sprintf(cmdbuf, 
        "nice -n 15 /home/pi/imgcomb -rgi /tmp/%s -therm -tf -im %s -thdat %s -o %s %s", 
        tmpfn,
        thparam, // therm_pal ? "-fire" : "",
        thermf, 
        fnh, 
        ampersand)
        ;   
    printf("%s\n", cmdbuf);
    // log_action(cmdbuf);
    // system(cmdbuf);
    
    char tmpsh[1024]; // = "./temprary.sh";
    sprintf(tmpsh, "/tmp/temp%d.sh", (unsigned)time(NULL) % 10);
    FILE* fsh = fopen(tmpsh, "w");
    
    fprintf(fsh, "while ps -ef | grep imgcomb | grep -v grep > /dev/null; do sleep 1; done\n");
    fprintf(fsh, "%s\n", cmdbuf);
    fprintf(fsh, "while [ ! -s %s ]\ndo\n", fnh);
    fprintf(fsh, "sleep 0.25\n");
    fprintf(fsh, "done\n");
    fprintf(fsh, "while ps -ef | grep imgcomb | grep -v grep > /dev/null; do sleep 1; done\n");
    
    
    
    char* noirarg;
    switch (ch_mapping)
    {   case 0:
        noirarg = "";       // rgi
        break;
        
        case 2:
        noirarg = "-im";    // monochrome
        break;
        
        case 3:
        noirarg = "-rig";   // vegetation
        break;
        
        case 4:
        noirarg = "-gri";   // red sky
        break;
        
        case 5:
        noirarg = "-igr";   // xmas
        break;
        
        case 6:
        noirarg = "-bw";    // blue and white
        break;
        
        case 1:
        default:
        noirarg = "-r";     // cir
    }
    
    sprintf(buffer, "Output filename: %s\n", fn);
    log_action(buffer);
    sprintf(cmdbuf, 
        "nice -n 15 /home/pi/imgcomb -rgi /tmp/%s %s -o %s %s", 
        tmpfn,
        noirarg, 
        fn, 
        ampersand
        );    
    log_action(cmdbuf);
    // printf("%s\n", cmdbuf);
    // system(cmdbuf);
    
    fprintf(fsh, "%s\n", cmdbuf);
    fprintf(fsh, "while [[ ! -s %s ]]\ndo\n", fn);
    fprintf(fsh, "sleep 0.25\n");
    fprintf(fsh, "done\n");
    
    fprintf(fsh, "sudo rm /tmp/%s\n", tmpfn);
    
    fclose(fsh);
    
    sprintf(cmdbuf, "nice -n 15 /bin/bash %s &", tmpsh);
    system(cmdbuf);
    
    
    // if (ch_mapping == 2) usleep(300000);
    
#if _SIMULSHOT
    ;
#else
    while (is_process_running("imgcomb")) usleep(50000);
    
    gpixph = draw_image_widget(gwidgetph, fnhth, 0);
    phxoff = lxoff;
    // usleep(100000);
    gpixp  = draw_image_widget(gwidgetp, fnth, 0);
    pxoff = lxoff;
#endif


  

  }
  
  system("gpio write 4 1");
  system("gpio write 0 1");
  firlit = fvlit = 0;
  
  // log_action("snapshot end\n\n");

  cairo_destroy (cr);
  
  flashd = 0;
  
  if (have_ip && !trupl)
  {  g_timeout_add_seconds(15, G_CALLBACK (force_gdbkp), NULL);
  }
    
    pics_taken++;
    if (pics_taken >= 5)
    {
        system("pkill ctrlr");
    }
  
  // if (tmlaps) g_timeout_add_seconds(tmlaps, G_CALLBACK (flash), NULL);
  return FALSE;         // false so the timer ftn does not recur.
}

static gboolean
flash(void)
{
  cairo_t *cr;
  char filename[1024];
  char buffer[1024];
  char tmpfn[256];
  
  if (last_flash > time(NULL) - 2) return FALSE;
  last_flash = time(NULL);
  
  if (flashd) return FALSE;
  else flashd = 1;
  
  imp++;
  impt++;
  
  if (flashv ) { system("gpio write 4 0"); firlit = 1; }
  if (flashir) { system("gpio write 0 0"); fvlit  = 1; }
    
  printf("Doing flash..\n");
  
  system("sudo mv /tmp/output.jpg \"/home/pi/Pictures/recovered$(date +\%Y\%m\%d.\%H\%M\%S).jpg\"");
  
  int tn;
  
  tn = (unsigned)time(NULL) & 0xfff;
  tn = tn << 8;
  tn += rand() & 0xff;
  sprintf(thermf, "/tmp/therm%i.dat", tn);
  
  FILE* fth = fopen(thermf, "wb");
  
  if (fth)
  { fwrite(thermdat, 768, sizeof(int), fth);
    fwrite(thermdat+1023, 1, sizeof(int), fth);
    fclose(fth);
  }
  
  // sprintf(buffer, "ps -ef | grep %i >> /home/pi/act.log", cam_pid);
  // system(buffer);
  // sprintf(buffer, "Killing SIGUSR1 to process %i", cam_pid);
  // log_action(buffer);
  kill(cam_pid, SIGUSR1);
  // log_action("Copying shared memory");

  /* if (pcntused >= 80) */ check_disk_usage();
  if (pcntused >= 98)
  {   offer_delete();
      return;
  }

  
  if (shutter < 13881503 || vid_on)
  {
    system("/home/pi/readshm 70 2048 int cp 73 768");
  } else
  { raspicam_cmd_format(buffer, 0, 0);
    raspistill_end_misery("taking long exposure");
    system(buffer);
    raspistill_init();
  }
  
  g_timeout_add(503, G_CALLBACK (flash_II), NULL);
  
  return TRUE;
}




void video_start()
{   cam_on = 0;
    raspistill_end_misery("taking video");
    system("sudo rm /tmp/thv*.bmp");
    thermiter=0;
    char cmdbuf[1024];
    raspicam_cmd_format(cmdbuf,0,1);
    printf("%s\n\n", cmdbuf);
    // system("/bin/bash /home/pi/escape.sh");
    system(cmdbuf);
    vid_on = 1;
}

void video_stop()
{   system("sudo pkill raspivid");

    GtkStyleContext *context = gtk_widget_get_style_context(recbtn);
    gtk_style_context_remove_class(context, "redbk");

    char buffer[1024], filename[1024];
    FILE* pf = popen("date +\x25Y\x25m\x25\x64.\x25H\x25M\x25S.\x25N", "r");
    if (pf)
    {   int lines = 0;
        while (fgets(buffer, 1024, pf))
        {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
            {   strcpy(filename, buffer);
                int i;
                for (i=0; i<1023; i++)
                {   if (filename[i] <= ' ') 
                    {   filename[i] = 0;
                        i += 1024;
                    }
                }
                sprintf(buffer, "Timestamp: %s\n", filename);
                // log_action(buffer);
                goto _thing;
                break;
            }
        }
        _thing:
        ;
        fclose(pf);
    }
  

    
    char cmdbuf[1024];
    sprintf(cmdbuf, 
            "MP4Box -add /tmp/output.h264 /home/pi/Videos/%s.mp4",
            // "MP4Box -add /tmp/output.flv /home/pi/Videos/%s.mp4",
            filename
           );
    system(cmdbuf);
    
    sprintf(cmdbuf,
            "ffmpeg -framerate 4 -i /tmp/thv%%06d.bmp /home/pi/Videos/%s_h.mp4",
            filename
           );
    system(cmdbuf);
    
    
    vid_on = 0;
    cam_on = 1;
    raspistill_init();
}


/******************************************************************************/
/* EVENT HANDLERS                                                             */
/******************************************************************************/

/* Create a new surface of the appropriate size */
static gboolean
configure_event_cb (GtkWidget         *widget,
                    GdkEventConfigure *event,
                    gpointer           data)
{
  if (surface)
    cairo_surface_destroy (surface);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surface ();
  
  gwidget = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_eventp_cb (GtkWidget         *widget,
                     GdkEventConfigure *event,
                     gpointer           data)
{
  if (surfacep)
    cairo_surface_destroy (surfacep);

  surfacep = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfacep ();
  
  gwidgetp = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_eventph_cb (GtkWidget         *widget,
                      GdkEventConfigure *event,
                      gpointer           data)
{
  if (surfaceph)
    cairo_surface_destroy (surfaceph);

  surfaceph = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfaceph ();
  
  gwidgetph = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_slsh_view_cb (GtkWidget         *widget,
                      GdkEventConfigure *event,
                      gpointer           data)
{
  if (surfacesv)
    cairo_surface_destroy (surfacesv);

  surfacesv = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfacev ();

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}


/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
  gcr = cr;
  if (!surface) return FALSE;
  
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}



static gboolean
drawp_cb (GtkWidget *widget,
          cairo_t   *cr,
          gpointer   data)
{ 
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  prevcr = cr;
  
  cairo_set_source_surface (cr, surfacep, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, gpixp, pxoff, 0);
  cairo_paint (cr);
  
  printf("drawp_cb()\n");

  return FALSE;
}


static gboolean
drawph_cb (GtkWidget *widget,
           cairo_t   *cr,
           gpointer   data)
{
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  prevhcr = cr;
  
  cairo_set_source_surface (cr, surfaceph, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, gpixph, phxoff, 0);
  cairo_paint (cr);

  return FALSE;
}


static gboolean
drawsv_cb (GtkWidget *widget,
           cairo_t   *cr,
           gpointer   data)
{
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  
  printf("Setting to surfacesv %i\n", surfacesv);
  cairo_set_source_surface (cr, surfacesv, 0, 0);
  printf("Setting to gpixsv %i\n", gpixsv);
  gdk_cairo_set_source_pixbuf (cr, gpixsv, shxoff, 0);
  cairo_paint (cr);

  return FALSE;
}


static void
close_window (void)
{   cam_on=0;
    raspistill_end_misery("closing main window");
  if (surface)
    cairo_surface_destroy (surface);
    surface = NULL;
}

void shutdown_screen();

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        data)
{
  /* paranoia check, in case we haven't gotten a configure event */
  if (surface == NULL)
    return FALSE;
    
    /*
  if (event->x && event->y && event->x < 16 && event->y < 16)
  {   char cmd[1024];
  
      shutdown_screen();
      
      system("sudo pkill raspistill");
      // system("sudo shutdown now");
      
      sprintf(cmd, "sudo kill %i", getpid());
      system(cmd);
      return FALSE;
  }*/
  
  if (devbox)
  { time_t rawtime;
    struct tm *info;
    char tmbuffer[80];

    time( &rawtime );

    info = localtime( &rawtime );

    strftime(tmbuffer,80,"%Y-%m-%d %H:%M:%S", info);
  
    FILE* fp = fopen("/home/pi/imtouched.log", "a");
    if (fp)
    {   fprintf(fp, "%s: %f, %f (%X) \n", 
                    tmbuffer, 
                    event->x, 
                    event->y,
                    event->state
               );
        fclose(fp);
    }
  }

  // if (event->state & GDK_BUTTON1_MASK)
    flash();

  /* We've handled it, stop processing */
  return TRUE;
}



void noirbtn_update(void)
{	/* gtk_button_set_label(noirbtn, 
                       ch_mapping ? "NoIR: CIR"
                                 : "NoIR: RGI"
                      );
                      */
  if (!noirbtn) return FALSE;
                      
  GtkStyleContext *context;
  context = gtk_widget_get_style_context(noirbtn);
  switch (ch_mapping)
  {   case 0:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "mono");      
      gtk_style_context_remove_class(context, "gri"); 
      gtk_style_context_remove_class(context, "igr"); 
      gtk_style_context_remove_class(context, "bw");
      gtk_style_context_add_class(context, "rgi");
      gtk_button_set_label(noirbtn, "NoIR: RGI");
      break;
      
      case 2:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "rgi");       
      gtk_style_context_remove_class(context, "gri"); 
      gtk_style_context_remove_class(context, "igr"); 
      gtk_style_context_remove_class(context, "bw");     
      gtk_style_context_add_class(context, "mono");
      gtk_button_set_label(noirbtn, "NoIR: Mon");
      break;
      
      case 3:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "rgi");       
      gtk_style_context_remove_class(context, "gri"); 
      gtk_style_context_remove_class(context, "igr"); 
      gtk_style_context_remove_class(context, "bw");    
      gtk_style_context_add_class(context, "veg");
      gtk_button_set_label(noirbtn, "NoIR: Veg");
      break;
      
      case 4:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "rgi");       
      gtk_style_context_remove_class(context, "veg"); 
      gtk_style_context_remove_class(context, "igr"); 
      gtk_style_context_remove_class(context, "bw");     
      gtk_style_context_add_class(context, "gri");
      gtk_button_set_label(noirbtn, "Red Sky");
      break;
      
      case 5:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "rgi"); 
      gtk_style_context_remove_class(context, "gri");      
      gtk_style_context_remove_class(context, "veg"); 
      gtk_style_context_remove_class(context, "bw");     
      gtk_style_context_add_class(context, "igr");
      gtk_button_set_label(noirbtn, "Xmas");
      break;
      
      case 6:
      gtk_style_context_remove_class(context, "cir");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "rgi"); 
      gtk_style_context_remove_class(context, "gri");      
      gtk_style_context_remove_class(context, "veg"); 
      gtk_style_context_remove_class(context, "igr");
      gtk_style_context_add_class(context, "bw");
      gtk_button_set_label(noirbtn, "InfraBlue");
      break;
  
      case 1:
      default:
      gtk_style_context_remove_class(context, "rgi");      
      gtk_style_context_remove_class(context, "gri");
      gtk_style_context_remove_class(context, "igr"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "bw");
      gtk_style_context_add_class(context, "cir");
      gtk_button_set_label(noirbtn, "NoIR: CIR");
  }
  gtk_widget_queue_draw(noirbtn);
    gtk_widget_queue_draw(previewh_area);
    gtk_widget_queue_draw(preview_area);
}

static gboolean exit_menu(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   
    gtk_window_present(window);
    gtk_window_close(menu);
    cam_on = 1;
    if (shutter > 999999) 
    {
        shutter = -1;
    }
    check_processes();
}

static gboolean
btnrgi_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 0;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btncir_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 1;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btnmon_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 2;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btnveg_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 3;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btnrs_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 4;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btnxms_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 5;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}

static gboolean
btnib_click(GtkWidget *widget,
                       GdkEventMotion *event,
                       gpointer        data)
{ 
    ch_mapping = 6;
    exit_menu(widget, event, data);
    noirbtn_update();
    save_settings();
}


static gboolean noirbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  chmap_menu();
  return TRUE;
}


void chmap_menu(GtkWidget *widget, GdkEventKey *key, int user_data)
{
    GtkStyleContext *context;
    
    printf("Turning off camera.\n");
    cam_on = 0;
    // slsh_lsidx = user_data;
    raspistill_end_misery("displaying channel map menu");
  
    printf("Creating menu window.\n");
    menu = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (menu), "Menu");
    
    printf("Full-screening menu window.\n");
    gtk_window_fullscreen(GTK_WINDOW(menu));
    gtk_container_set_border_width (GTK_CONTAINER (menu), 0);
    
    printf("Connecting Esc keypress event.\n");
    g_signal_connect (menu, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

    // Get the current month for seasonal options.
    FILE* pf = popen("date +\x25m", "r");
    char buffer[1024];
    int month=0;
    if (pf)
    { int j = 0;
      while (fgets(buffer, 1024, pf))
      {   if (j = atoi(buffer))               // assignment, not comparison
            month = j;
      }
      fclose(pf);
    }
    
    // Big grid
    printf("Creating menu grid.\n");
    menu_grid = gtk_grid_new();
    gtk_widget_set_size_request(menu_grid, SCR_RES_X, SCR_RES_Y);
    gtk_container_add (GTK_CONTAINER (menu), menu_grid);
    
    int w, h;
    printf("Getting size.\n");
    gtk_window_get_size(menu, &w, &h);
    
    printf("Adding buttons.\n");
    menu_btnrgi = gtk_button_new_with_label("RGI");
    gtk_grid_attach(menu_grid, menu_btnrgi, 0, 0, 2, 1);
    context = gtk_widget_get_style_context(menu_btnrgi);
    gtk_style_context_add_class(context, "rgibig");
    
    printf("Adding buttons.\n");
    menu_btncir = gtk_button_new_with_label("CIR");
    gtk_grid_attach(menu_grid, menu_btncir, 2, 0, 2, 1);
    context = gtk_widget_get_style_context(menu_btncir);
    gtk_style_context_add_class(context, "cirbig");
    
    menu_btnmon = gtk_button_new_with_label("Mono");
    gtk_grid_attach(menu_grid, menu_btnmon, 4, 0, 2, 1);
    context = gtk_widget_get_style_context(menu_btnmon);
    gtk_style_context_add_class(context, "monbig");
    
    GtkWidget* menu_spacer1 = gtk_button_new_with_label("");
    gtk_grid_attach(menu_grid, menu_spacer1, 0, 1, 1, 2);
    
    menu_btnveg = gtk_button_new_with_label("Veg");
    gtk_grid_attach(menu_grid, menu_btnveg, 1, 1, 2, 1);
    context = gtk_widget_get_style_context(menu_btnveg);
    gtk_style_context_add_class(context, "vegbig");
    
    menu_btnrs = gtk_button_new_with_label("Rdsky");
    gtk_grid_attach(menu_grid, menu_btnrs, 3, 1, 2, 1);
    context = gtk_widget_get_style_context(menu_btnrs);
    gtk_style_context_add_class(context, "rsbig");
    
    GtkWidget* menu_spacer2 = gtk_button_new_with_label("");
    gtk_grid_attach(menu_grid, menu_spacer2, 5, 1, 1, 2);
    
    if (month == 12)
    {
        menu_btnxms = gtk_button_new_with_label("Xmas");
        gtk_grid_attach(menu_grid, menu_btnxms, 1, 2, 2, 1);
        context = gtk_widget_get_style_context(menu_btnxms);
        gtk_style_context_add_class(context, "xmsbig");
    
        menu_btnib = gtk_button_new_with_label("InfrBlu");
        gtk_grid_attach(menu_grid, menu_btnib, 3, 2, 2, 1);
        context = gtk_widget_get_style_context(menu_btnib);
        gtk_style_context_add_class(context, "ibbig");
    }
    
    
    gtk_widget_set_size_request(menu_btnrgi, 
                                SCR_RES_X/3, 
                                SCR_RES_Y/3
                               );
                               
    gtk_widget_set_size_request(menu_btncir, 
                                SCR_RES_X/3, 
                                SCR_RES_Y/3
                               );
    
    gtk_widget_set_size_request(menu_btnmon, 
                                SCR_RES_X/3, 
                                SCR_RES_Y/3
                               );
    
    gtk_widget_set_size_request(menu_btnveg, 
                                SCR_RES_X/3, 
                                SCR_RES_Y/3
                               );
                               
    gtk_widget_set_size_request(menu_btnrs, 
                                SCR_RES_X/3, 
                                SCR_RES_Y/3
                               );
    
    gtk_widget_set_size_request(menu_spacer1, 
                                SCR_RES_X/6, 
                                SCR_RES_Y/3
                               );
    
    gtk_widget_set_size_request(menu_spacer2, 
                                SCR_RES_X/6, 
                                SCR_RES_Y/3
                               );
    
    if (month == 12)
    {
        gtk_widget_set_size_request(menu_btnxms, 
                                    SCR_RES_X/3, 
                                    SCR_RES_Y/3
                                   );
                                   
        gtk_widget_set_size_request(menu_btnib, 
                                    SCR_RES_X/3, 
                                    SCR_RES_Y/3
                                   );
    }
    
    
    printf("Connecting button events.\n");
    
    g_signal_connect(menu_btnrgi, "button-press-event",
                      G_CALLBACK (btnrgi_click), NULL);
                      
    g_signal_connect(menu_btncir, "button-press-event",
                      G_CALLBACK (btncir_click), NULL);
    
    g_signal_connect(menu_btnveg, "button-press-event",
                      G_CALLBACK (btnveg_click), NULL);
    
    g_signal_connect(menu_btnmon, "button-press-event",
                      G_CALLBACK (btnmon_click), NULL);
    
    g_signal_connect(menu_btnrs, "button-press-event",
                      G_CALLBACK (btnrs_click), NULL);
    
    if (month == 12)
    {
        g_signal_connect(menu_btnxms, "button-press-event",
                          G_CALLBACK (btnxms_click), NULL);
    
        g_signal_connect(menu_btnib, "button-press-event",
                          G_CALLBACK (btnib_click), NULL);
    }
      
    
    printf("Showing slideshow window.\n");
    gtk_widget_show_all(menu);
}


void thbtn_update(void)
{	char thnm[16], thcss[16];
    
    switch (therm_pal)
    {	case _THM_FIRE:
	    strcpy(thnm, "Thermal: Fire");
	    strcpy(thcss, "fire");
	    break;
	    
	    case _THM_FEVR:
	    strcpy(thnm, "Thermal: Fever");
	    strcpy(thcss, "fever");
	    break;
	    
	    case _THM_ROOM:
	    strcpy(thnm, "Thermal: Room");
	    strcpy(thcss, "room");
	    break;
	    
	    case _THM_AMB:
	    strcpy(thnm, "Thermal: Ambient");
	    strcpy(thcss, "amb");
	    break;
	    
	    case _THM_RAINB:
	    strcpy(thnm, "Thermal: Rainbow");
	    strcpy(thcss, "rainb");
	    break;
	    
	    case _THM_BLEU:
	    strcpy(thnm, "Thermal: Bleu");
	    strcpy(thcss, "bleu");
	    break;
	    
	    case _THM_TIV:
	    strcpy(thnm, "Thermal: T-I-V");
	    strcpy(thcss, "tiv");
	    break;
	    
	    case _THM_LAVA:
	    strcpy(thnm, "Thermal: Lavaworld");
	    strcpy(thcss, "lava");
	    break;
	    
	    case _THM_HUE:
	    default:
	    strcpy(thnm, "Thermal: Hues");
	    strcpy(thcss, "hues");
	    break;
    }
  
  gtk_button_set_label(thbtn, 
                       thnm
                      );
  GtkStyleContext *context;
  context = gtk_widget_get_style_context(thbtn);
  
  
  
  /*if (therm_pal)
  {   gtk_style_context_remove_class(context, "hues");
      gtk_style_context_add_class(context, "fire");
  }
  else
  {   gtk_style_context_remove_class(context, "fire");
      gtk_style_context_add_class(context, "hues");
  }*/
  
  gtk_style_context_remove_class(context, "fire");
  gtk_style_context_remove_class(context, "fever");
  gtk_style_context_remove_class(context, "rainb");
  gtk_style_context_remove_class(context, "bleu");
  gtk_style_context_remove_class(context, "hues");
  gtk_style_context_remove_class(context, "tiv");
  gtk_style_context_remove_class(context, "lava");
  gtk_style_context_remove_class(context, "room");
  gtk_style_context_remove_class(context, "amb");
    
  gtk_style_context_add_class(context, thcss);
  
  gtk_widget_queue_draw(thbtn);
}

static gboolean thbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ /*therm_pal++;
  if (therm_pal > _THM_MAX) therm_pal = 0;
  
  thbtn_update();
  save_settings();*/
  
  therm_menu();
  
  return TRUE;
}

static gboolean mnufire_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 0;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnurnbo_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 1;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnufevr_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 2;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnuroom_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 3;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnuamb_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 4;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnuhue_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 5;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnubleu_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 6;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

static gboolean mnutiv_click(GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
    therm_pal = 7;
    exit_menu(widget, event, data);
    thbtn_update();
    save_settings();
}

void therm_menu(GtkWidget *widget, GdkEventKey *key, int user_data)
{
    GtkStyleContext *context;
    
    printf("Turning off camera.\n");
    cam_on = 0;
    // slsh_lsidx = user_data;
    raspistill_end_misery("displaying thermal menu");
  
    printf("Creating menu window.\n");
    menu = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (menu), "Menu");
    
    printf("Full-screening menu window.\n");
    gtk_window_fullscreen(GTK_WINDOW(menu));
    gtk_container_set_border_width (GTK_CONTAINER (menu), 0);
    
    printf("Connecting Esc keypress event.\n");
    g_signal_connect (menu, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

    int rows = 4;
    
    // Big grid
    printf("Creating menu grid.\n");
    menu_grid = gtk_grid_new();
    gtk_widget_set_size_request(menu_grid, SCR_RES_X, SCR_RES_Y);
    gtk_container_add (GTK_CONTAINER (menu), menu_grid);
    
    int w, h;
    printf("Getting size.\n");
    gtk_window_get_size(menu, &w, &h);
    
    printf("Adding buttons.\n");
    menu_fire = gtk_button_new_with_label("Thermal: Fire");
    gtk_grid_attach(menu_grid, menu_fire, 0, 0, 1, 1);
    context = gtk_widget_get_style_context(menu_fire);
    gtk_style_context_add_class(context, "fire");
    
    menu_rnbo = gtk_button_new_with_label("Thermal: Rainbow");
    gtk_grid_attach(menu_grid, menu_rnbo, 1, 0, 1, 1);
    context = gtk_widget_get_style_context(menu_rnbo);
    gtk_style_context_add_class(context, "rainb");
    
    menu_fevr = gtk_button_new_with_label("Thermal: Fever");
    gtk_grid_attach(menu_grid, menu_fevr, 0, 1, 1, 1);
    context = gtk_widget_get_style_context(menu_fevr);
    gtk_style_context_add_class(context, "fever");
    
    menu_room = gtk_button_new_with_label("Thermal: Room");
    gtk_grid_attach(menu_grid, menu_room, 1, 1, 1, 1);
    context = gtk_widget_get_style_context(menu_room);
    gtk_style_context_add_class(context, "room");
    
    menu_amb = gtk_button_new_with_label("Thermal: Ambient");
    gtk_grid_attach(menu_grid, menu_amb, 0, 2, 1, 1);
    context = gtk_widget_get_style_context(menu_amb);
    gtk_style_context_add_class(context, "amb");
    
    menu_hue = gtk_button_new_with_label("Thermal: Hues");
    gtk_grid_attach(menu_grid, menu_hue, 1, 2, 1, 1);
    context = gtk_widget_get_style_context(menu_hue);
    gtk_style_context_add_class(context, "hues");
    
    menu_bleu = gtk_button_new_with_label("Thermal: Bleu");
    gtk_grid_attach(menu_grid, menu_bleu, 0, 3, 1, 1);
    context = gtk_widget_get_style_context(menu_bleu);
    gtk_style_context_add_class(context, "bleu");
    
    menu_tiv = gtk_button_new_with_label("Thermal: TIV");
    gtk_grid_attach(menu_grid, menu_tiv, 1, 3, 1, 1);
    context = gtk_widget_get_style_context(menu_tiv);
    gtk_style_context_add_class(context, "tiv");
    
    int vpad = 1;
    
    gtk_widget_set_size_request(menu_fire, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_rnbo, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_fevr, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_amb, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_hue, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_bleu, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_room, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    gtk_widget_set_size_request(menu_tiv, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/rows-vpad
                               );
    
    
    printf("Connecting button events.\n");
    
    g_signal_connect(menu_fire, "button-press-event",
                      G_CALLBACK (mnufire_click), NULL);
    
    g_signal_connect(menu_rnbo, "button-press-event",
                      G_CALLBACK (mnurnbo_click), NULL);
    
    g_signal_connect(menu_fevr, "button-press-event",
                      G_CALLBACK (mnufevr_click), NULL);
    
    g_signal_connect(menu_room, "button-press-event",
                      G_CALLBACK (mnuroom_click), NULL);
    
    g_signal_connect(menu_amb, "button-press-event",
                      G_CALLBACK (mnuamb_click), NULL);
    
    g_signal_connect(menu_hue, "button-press-event",
                      G_CALLBACK (mnuhue_click), NULL);
    
    g_signal_connect(menu_bleu, "button-press-event",
                      G_CALLBACK (mnubleu_click), NULL);
    
    g_signal_connect(menu_tiv, "button-press-event",
                      G_CALLBACK (mnutiv_click), NULL);
    
    printf("Showing slideshow window.\n");
    gtk_widget_show_all(menu);
}



void
actually_shudown_computer(void)
{   cam_on=0;
    raspistill_end_misery("shutting down");
    usleep(531981);
    system("sudo pkill fbcp");
    usleep(531981);
    system("sudo shutdown now");
}


static gboolean xbtn_click(GtkWidget      *widget,
                           GdkEventButton *event,
                           gpointer        data)
{ if (event->x = lxbx && event->y == lxby) 
  { lxbx = event->x;
    lxby = event->y;
    return TRUE;
  }
  
  if (flashd) return TRUE;

  lxbx = event->x;
  lxby = event->y;
  
  char jlgsux[1024];
  sprintf(jlgsux, "xbtn_click ( %i, %i )", event->x, event->y);
  log_action(jlgsux);

  if (pwrr) shutdown_screen();
  else 
  {	pwrr = 17;
  	upd_btn_pwr();
  }
  return TRUE;
}
// exposure and resolution buttons: call raspistill_init();


static gboolean resmbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{      if (camres <=  600) camres = 480;
  else if (camres <=  768) camres = 600;
  else if (camres <= 1080) camres = 768;
  
  upd_lbl_res();
  save_settings();
  raspistill_init();
  return TRUE;
}



static gboolean respbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{      if (camres >=  768) camres = 1080;   // something is wrong with imgcomb
  else if (camres >=  600) camres =  768;
  else if (camres >=  480) camres =  600;
  
  upd_lbl_res();
  save_settings();
  raspistill_init();
  return TRUE;
}



static gboolean shutmbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  switch (shutter)
  { case 3: shutter=-1; break;
    case 10: shutter=3; break;
    case 33: shutter=10; break;
    case 100: shutter=33; break;
    case 333: shutter=100; break;
    case 1000: shutter=333; break;
    case 3000: shutter=1000; break;
    case 10000: shutter=3000; break;
    case 20000: shutter=10000; break;
    case 50000: shutter=20000; break;
    case 100000: shutter=50000; break;
    case 166667: shutter=100000; break;
    case 250000: shutter=166667; break;
    case 500000: shutter=250000; break;
    case 750000: shutter=500000; break;
    case 1000000: shutter=750000; break;
    case 2000000: shutter=1000000; break;
    case 3500000: shutter=2000000; break;
    case 5889950: shutter=3500000; break;
    default: shutter=100000; break;
  }
  
  upd_lbl_shut();
  save_settings();
  ctdn2camreinit = 13;
  // raspistill_init();
  return TRUE;
}

static gboolean shutpbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  switch (shutter)
  { case 3: shutter=10; break;
    case 10: shutter=33; break;
    case 33: shutter=100; break;
    case 100: shutter=333; break;
    case 333: shutter=1000; break;
    case 1000: shutter=3000; break;
    case 3000: shutter=10000; break;
    case 10000: shutter=20000; break;
    case 20000: shutter=50000; break;
    case 50000: shutter=100000; break;
    case 100000: shutter=166667; break;
    case 166667: shutter=250000; break;
    case 250000: shutter=500000; break;
    case 500000: shutter=750000; break;
    case 750000: shutter=1000000; break;
    case 1000000: shutter=2000000; break;
    case 2000000: shutter=3500000; break;
    case 3500000: shutter=5889950; break;
    case 5889950: shutter=5889950; break;
    default: shutter=100000; break;
  }
  
  upd_lbl_shut();
  save_settings();
  ctdn2camreinit = 13;
  // raspistill_init();
  return TRUE;
}



static gboolean shutauto_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  if (shutter > 3000000)
  { shutter=100000;
    raspistill_init();
  }


  shutter=-1;
  save_settings();
  
  upd_lbl_shut();
  raspistill_init();
  return TRUE;
}


static gboolean explbl_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   expmode++;
    if (expmode > 8) expmode = 0;
    
    save_settings();
    upd_lbl_exp();
    
    ctdn2camreinit = 13;
}


static gboolean flashon_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  if (flashv  && !fvlit ) { system("gpio write 4 0"); fvlit  = 1; }
  else                    { system("gpio write 4 1"); fvlit  = 0; }
  
  if (flashir && !firlit) { system("gpio write 0 0"); firlit = 1; }
  else                    { system("gpio write 0 1"); firlit = 0; }
  save_settings();
  
  return TRUE;
}

static gboolean recbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  if (vid_on) video_stop();
  else video_start();
  
  return TRUE;
}

static gboolean slsh_xbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   
    gtk_window_present(window);
    gtk_window_close(slideshow);
    cam_on = 1;
    if (shutter > 999999) 
    {   shutter = -1;
        // upd_lbl_shut();
    }
    check_processes();
}

void
delete_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   // printf("Response: %i\n", response_id);
    // system("sudo pkill raspistill");
    // kill(getpid(),SIGINT);
    
    if (response_id == -8)          // yes
    {   char cmdbuf[1024];
        if (!slsh_cimg[0]) return;
        
        sprintf(cmdbuf, "sudo rm %s", slsh_cimg);
        system(cmdbuf);
        slsh_view_file();
    }
}

static gboolean slsh_dbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   GtkWidget * dlg = gtk_message_dialog_new (slideshow,
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_WARNING,
                        GTK_BUTTONS_YES_NO,
                        "Really DELETE this photo?"
                        );
                        
    g_signal_connect(dlg, "response",
                      G_CALLBACK (delete_response), NULL);
    
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}

static gboolean slsh_lbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   if (slsh_lsidx) slsh_lsidx--;
    else slsh_view_oldest();
    printf("slsh_lsidx = %i\n", slsh_lsidx);
    slsh_view_file();
}


void ifbtn_upd(void)
{    GtkStyleContext *context;
    context = gtk_widget_get_style_context(ifbtn);
    if (flashir)
    {   gtk_style_context_add_class(context, "ifon");
    }
    else
    {   gtk_style_context_remove_class(context, "ifon");
    }
}

static gboolean ifbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   flashir = 1-flashir;
	ifbtn_upd();
  	save_settings();
}

void vfbtn_upd(void)
{	GtkStyleContext *context;
    context = gtk_widget_get_style_context(vfbtn);
    if (flashv)
    {   gtk_style_context_add_class(context, "vfon");
    }
    else
    {   gtk_style_context_remove_class(context, "vfon");
    }
}

static gboolean vfbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   flashv = 1-flashv;
    vfbtn_upd();
  	save_settings();
}



static gboolean slsh_rbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   slsh_lsidx++;
    printf("slsh_lsidx = %i\n", slsh_lsidx);
    slsh_view_file();
}

int
window_key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer user_data) 
{   
    if (key->keyval == GDK_KEY_Escape)
    {   /*system("sudo pkill unclutter");
        system("sudo kill \"$(< /tmp/czpid)\"");
        cam_on =0;
        raspistill_end_misery("exiting app");
        kill(getpid(),SIGINT);
        system("sudo pkill ctrlr");*/
        system("/bin/bash /home/pi/escape.sh");
        return TRUE;
    }
    
    if (key->keyval == GDK_KEY_w)
    {   therm_off_y -= 0.01;
        save_thalign();
    }

    if (key->keyval == GDK_KEY_z)
    {   therm_off_y += 0.01;
        save_thalign();
    }
    
    if (key->keyval == GDK_KEY_a)
    {   therm_off_x -= 0.01;
        save_thalign();
    }

    if (key->keyval == GDK_KEY_s)
    {   therm_off_x += 0.01;
        save_thalign();
    }
    
    if (key->keyval == GDK_KEY_q)
    {   therm_rot_rad -= 0.001;
        save_thalign();
    }

    if (key->keyval == GDK_KEY_e)
    {   therm_rot_rad += 0.001;
        save_thalign();
    }
   
    if (key->keyval == GDK_KEY_space)
    {   flash();
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_BackSpace)
    {   slsh_xbut_click(slsh_xbut, NULL, NULL);
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_leftarrow)
    {   slsh_xbut_click(slsh_lbut, NULL, NULL);
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_rightarrow)
    {   slsh_xbut_click(slsh_rbut, NULL, NULL);
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_j)
    {   do                          // deliberate infinite loop for test
        {   ;
        }   while (key->keyval == GDK_KEY_j);
    }
    
    return FALSE;       // avoid swallow unknown keystrokes
}



/******************************************************************************/
/* SHIT THAT RUNS WHEN WE OPEN THE WINDOW LET SOME AIR IN THIS MOTHERFUCKER   */
/******************************************************************************/


void shutdown_screen()
{   GtkWidget *shitdeun = gtk_application_window_new (app);
        
    gtk_window_fullscreen(GTK_WINDOW(shitdeun));
    gtk_container_set_border_width (GTK_CONTAINER (shitdeun), 0);
    
    GtkWidget *shitlbl = 
        gtk_label_new(
            "Shutting down....\n\nWhen the green light turns OFF and stays off, it is\nsafe to flip the power switch."
                     );
                  
    gtk_container_add (GTK_CONTAINER (shitdeun), shitlbl); 
    
     
    gtk_widget_show_all(shitdeun); 
    
    cam_on=0;
    raspistill_end_misery("displaying shitdeun screen");
    
    g_timeout_add(2537, G_CALLBACK (actually_shudown_computer), NULL);
}


void open_slideshow(GtkWidget *widget, GdkEventKey *key, int user_data)
{
    
    GtkStyleContext *context;
    
    printf("Turning off camera.\n");
    cam_on = 0;
    slsh_lsidx = user_data;
    raspistill_end_misery("displaying slideshow");
  
    printf("Creating slideshow window.\n");
    slideshow = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (slideshow), "Slideshow");
    
    printf("Full-screening slideshow window.\n");
    gtk_window_fullscreen(GTK_WINDOW(slideshow));
    gtk_container_set_border_width (GTK_CONTAINER (slideshow), 0);
    
    printf("Connecting Esc keypress event.\n");
    g_signal_connect (slideshow, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

    
    // Big grid
    printf("Creating slideshow grid.\n");
    slsh_grid = gtk_grid_new();
    gtk_widget_set_size_request(slsh_grid, SCR_RES_X, SCR_RES_Y);
    gtk_container_add (GTK_CONTAINER (slideshow), slsh_grid);
    
    /*
    GtkWidget *slsh_lbut;
    GtkWidget *slsh_rbut;
    GtkWidget *slsh_view;
    GtkWidget *slsh_xbut;
    */
    
    int w, h;
    printf("Getting size.\n");
    gtk_window_get_size(slideshow, &w, &h);
    
    printf("Adding buttons.\n");
    slsh_lbut = gtk_button_new_with_label("<<");
    gtk_grid_attach(slsh_grid, slsh_lbut, 0, 0, 1, 1);
    
    slsh_rbut = gtk_button_new_with_label(">>");
    gtk_grid_attach(slsh_grid, slsh_rbut, 2, 0, 1, 1);
    
    slsh_dbut = gtk_button_new_with_label("DEL");
    gtk_grid_attach(slsh_grid, slsh_dbut, 3, 0, 1, 1);
    
    context = gtk_widget_get_style_context(slsh_dbut);
    gtk_style_context_add_class(context, "redbk");
    
    slsh_xbut = gtk_button_new_with_label("Back to Camera");
    gtk_grid_attach(slsh_grid, slsh_xbut, 1, 0, 1, 1);
    
    printf("Connecting button events.\n");
    g_signal_connect(slsh_xbut, "button-press-event",
                      G_CALLBACK (slsh_xbut_click), NULL);
    
    g_signal_connect(slsh_lbut, "button-press-event",
                      G_CALLBACK (slsh_lbut_click), NULL);
    
    g_signal_connect(slsh_rbut, "button-press-event",
                      G_CALLBACK (slsh_rbut_click), NULL);
    
    g_signal_connect(slsh_dbut, "button-press-event",
                      G_CALLBACK (slsh_dbut_click), NULL);
    
    slsh_view = gtk_drawing_area_new();
    g_signal_connect (slsh_view,"configure-event",
                    G_CALLBACK (configure_slsh_view_cb), NULL);

    printf("Sizing view.\n");
    gtk_widget_set_size_request(slsh_view, 
                                SCR_RES_X, 
                                SCR_RES_Y-81
                               );
                               
    printf("Connecting draw event.\n");
    g_signal_connect (slsh_view, "draw",
                      G_CALLBACK (drawsv_cb), NULL);
                               
    printf("Attaching grid.\n");
    gtk_grid_attach(slsh_grid, slsh_view, 0, 1, 4, 1);
   
    
    
    slsh_flbl = gtk_label_new("");
    gtk_grid_attach(slsh_grid, slsh_flbl, 0, 2, 4, 1);
    
    context = gtk_widget_get_style_context(slsh_flbl);
    gtk_style_context_add_class(context, "tinytxt");
    
    
    printf("Showing slideshow window.\n");
    gtk_widget_show_all(slideshow);
    printf("Displaying initial image file.\n");
    slsh_view_file();
}




static void
activate (GtkApplication* app,
          gpointer        user_data)
{
  GtkStyleContext *context;
  
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Camera Control");
  
  
  system("unclutter -idle 0 &");
  
  
  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  
  /*
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_widget_set_size_request(frame, SCR_RES_X, SCR_RES_Y);
  gtk_container_add (GTK_CONTAINER (window), frame);
  */
  
  grid = gtk_grid_new();
  GtkWidget *thipgrid = gtk_grid_new();
  
  
  
  gtk_widget_set_size_request(grid, SCR_RES_X, SCR_RES_Y);
  gtk_container_add (GTK_CONTAINER (window), grid);
  
  // cell 0,0 NoIR type
  noirbtn = gtk_button_new_with_label("NoIR: CIR");
  gtk_grid_attach(grid, noirbtn, 0, 0, 1, 1);
  
  context = gtk_widget_get_style_context(noirbtn);
  gtk_style_context_add_class(context, "cir");
  
  gtk_widget_set_size_request(noirbtn, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (noirbtn, "button-press-event",
                    G_CALLBACK (noirbtn_click), NULL);
  
  // cell 1,0 thermal type
  gtk_grid_attach(grid, thipgrid, 1, 0, 1, 1);
  
  thbtn = gtk_button_new_with_label("Thermal: fire");
  gtk_grid_attach(thipgrid, thbtn, 0, 0, 1, 1);
  
  iplbl = gtk_label_new("IP Address");
  
  context = gtk_widget_get_style_context(iplbl);
  gtk_style_context_add_class(context, "tinytxt");
  gtk_grid_attach(thipgrid, iplbl, 0, 1, 1, 1);
  
  context = gtk_widget_get_style_context(thbtn);
  gtk_style_context_add_class(context, "fire");
  
  gtk_widget_set_size_request(thbtn, 
                              SCR_RES_X/2, 
                              SCR_RES_Y/6
                              );
  
  g_signal_connect (thbtn, "button-press-event",
                    G_CALLBACK (thbtn_click), NULL);
  
  // cell 2,0 exit

  xbtn = gtk_button_new_with_label("");		// "OFF"
  gtk_grid_attach(grid, xbtn, 2, 0, 1, 1);
  
  context = gtk_widget_get_style_context(xbtn);
  gtk_style_context_add_class(context, "pwr");		// "redbk"
  
  gtk_widget_set_size_request(xbtn, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (xbtn, "button-press-event",
                    G_CALLBACK (xbtn_click), NULL);
  
  // cell 0,1 preview

  prevgrid = gtk_grid_new();
  gtk_grid_attach(grid, prevgrid, 0, 1, 1, 1);
  
  preview_area = gtk_drawing_area_new ();
  // preview_area = gtk_image_new ();
  gtk_widget_set_size_request(preview_area, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  gtk_grid_attach(prevgrid, preview_area, 0, 0, 1, 1);

  
  previewh_area = gtk_drawing_area_new ();
  // previewh_area = gtk_image_new ();
  gtk_widget_set_size_request(previewh_area, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  gtk_grid_attach(prevgrid, previewh_area, 0, 1, 1, 1);

  // cell 1,1 live image
  drawing_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request(drawing_area, SCR_RES_X/2, SCR_RES_Y/2);
  gtk_grid_attach(grid, drawing_area, 1, 1, 1, 1);
  
  // cell 2,1 flash setting
  
  GtkWidget *flgrid = gtk_grid_new();
  gtk_grid_attach(grid, flgrid, 2, 1, 1, 1);
  
  ifbtn = gtk_button_new_with_label("Flash IR");
  vfbtn = gtk_button_new_with_label("Flash Vis");
  
  gtk_grid_attach(flgrid, ifbtn, 0, 0, 1, 1);
  gtk_grid_attach(flgrid, vfbtn, 0, 1, 1, 1);
  
  int ysz = allowvid
          ? SCR_RES_Y/6
          : SCR_RES_Y/4;
  
  gtk_widget_set_size_request(ifbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
                              
  gtk_widget_set_size_request(vfbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
  
  context = gtk_widget_get_style_context(ifbtn);
  gtk_style_context_add_class(context, "ifon");
  
  g_signal_connect (ifbtn, "button-press-event",
                    G_CALLBACK (ifbtn_click), NULL);
  
  g_signal_connect (vfbtn, "button-press-event",
                    G_CALLBACK (vfbtn_click), NULL);
  
  
  // cell 0,2 resolution

  resgrid = gtk_grid_new();
  gtk_grid_attach(grid, resgrid, 0, 2, 1, 1);
  

  resmbtn = gtk_button_new_with_label("-");
  reslbl = gtk_label_new("800\nx600");
  respbtn = gtk_button_new_with_label("+");
  gtk_grid_attach(resgrid, reslbl, 1, 0, 1, 1);
  gtk_grid_attach_next_to(resgrid, resmbtn, reslbl, GTK_POS_LEFT, 1, 1);
  gtk_grid_attach_next_to(resgrid, respbtn, reslbl, GTK_POS_RIGHT, 1, 1);
  
  context = gtk_widget_get_style_context(reslbl);
  gtk_style_context_add_class(context, "smtxt");
  
  
  gtk_widget_set_size_request(reslbl, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/4
                              );
                              
  gtk_widget_set_size_request(resmbtn, 
                              SCR_RES_X/16, 
                              SCR_RES_Y/4
                              );
                              
  gtk_widget_set_size_request(respbtn, 
                              SCR_RES_X/16, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (resmbtn, "button-press-event",
                    G_CALLBACK (resmbtn_click), NULL);
  
  g_signal_connect (respbtn, "button-press-event",
                    G_CALLBACK (respbtn_click), NULL);
  
  
  // cell 1,2 shutter
  
  shexpgrid = gtk_grid_new();
  gtk_grid_attach(grid, shexpgrid, 1, 2, 1, 1);

  shutgrid = gtk_grid_new();
  gtk_grid_attach(shexpgrid, shutgrid, 0, 0, 1, 1);
  
  shutmbtn = gtk_button_new_with_label("-");
  
  // shutlbl = gtk_label_new("Shutter:\nAuto");
  shutlbl = gtk_button_new_with_label("Sh:\nAuto");
  
  shutpbtn = gtk_button_new_with_label("+");
  gtk_grid_attach(shutgrid, shutlbl, 1, 0, 1, 1);
  gtk_grid_attach_next_to(shutgrid, shutmbtn, shutlbl, GTK_POS_LEFT, 1, 1);
  gtk_grid_attach_next_to(shutgrid, shutpbtn, shutlbl, GTK_POS_RIGHT, 1, 1);
  
  gtk_widget_set_size_request(shutlbl, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/8
                              );
                              
  gtk_widget_set_size_request(shutmbtn, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/8
                              );
                              
  gtk_widget_set_size_request(shutpbtn, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/8
                              );
  
  g_signal_connect (shutmbtn, "button-press-event",
                    G_CALLBACK (shutmbtn_click), NULL);
  
  g_signal_connect (shutpbtn, "button-press-event",
                    G_CALLBACK (shutpbtn_click), NULL);
          
          
                    
  explbl = gtk_button_new_with_label("Exp: auto"); 
  gtk_grid_attach(shexpgrid, explbl, 0, 1, 1, 1);             
  
  gtk_widget_set_size_request(explbl, 
                              SCR_RES_X/2, 
                              SCR_RES_Y/8
                              );
                              
  g_signal_connect (explbl, "button-press-event",
                    G_CALLBACK (explbl_click), NULL);                            
  
  
  
  // cell 2,2 flash on
  // flonbtn = gtk_button_new_with_label("Auto\nShutter");       // shaut aup
  flonbtn = gtk_button_new_with_label("Flash On");
  ysz = allowvid
          ? SCR_RES_Y/6
          : SCR_RES_Y/4;
  
  gtk_widget_set_size_request(flonbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
  
  // cell 2,2 alternate: record btn               
  recbtn  = gtk_button_new_with_label("REC");

  if (allowvid)
  { gtk_grid_attach(flgrid, flonbtn, 0, 2, 1, 1);
    gtk_grid_attach(grid, recbtn, 2, 2, 1, 1);
  }
  else
    gtk_grid_attach(grid, flonbtn, 2, 2, 1, 1);
  
  g_signal_connect (shutlbl, "button-press-event",
                    G_CALLBACK (shutauto_click), NULL);
  
  g_signal_connect (flonbtn, "button-press-event",
                    G_CALLBACK (flashon_click), NULL);
                    
  g_signal_connect (recbtn, "button-press-event",
                    G_CALLBACK (recbtn_click), NULL);
                    
                    

  /* Signals used to handle the backing surface */
  g_signal_connect (drawing_area, "draw",
                    G_CALLBACK (draw_cb), NULL);
                    
  g_signal_connect (preview_area, "draw",
                    G_CALLBACK (drawp_cb), NULL);
                    
  g_signal_connect (previewh_area, "draw",
                    G_CALLBACK (drawph_cb), NULL);
                    
                    
  g_signal_connect (drawing_area,"configure-event",
                    G_CALLBACK (configure_event_cb), NULL);
                    
  g_signal_connect (preview_area,"configure-event",
                    G_CALLBACK (configure_eventp_cb), NULL);
                    
  g_signal_connect (previewh_area,"configure-event",
                    G_CALLBACK (configure_eventph_cb), NULL);
                    
  
  
  
  g_timeout_add(274, G_CALLBACK (thermdraw), NULL);
  check_processes();
  g_timeout_add(1000, G_CALLBACK (check_processes), NULL);
  g_timeout_add(1003, G_CALLBACK (redraw_btns), NULL);
  
  g_signal_connect (drawing_area, "button-press-event",
                    G_CALLBACK (motion_notify_event_cb), NULL);

  g_signal_connect (preview_area, "button-press-event",
                    G_CALLBACK (open_slideshow), -1);
                    
  g_signal_connect (previewh_area, "button-press-event",
                    G_CALLBACK (open_slideshow), -2);
                    
                    
                    
  gtk_widget_set_events (drawing_area, gtk_widget_get_events (drawing_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_BUTTON_RELEASE_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
  gtk_widget_set_events (preview_area, gtk_widget_get_events (preview_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);

  gtk_widget_set_events (previewh_area, gtk_widget_get_events (previewh_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
  gtk_widget_set_events (shutlbl, gtk_widget_get_events (shutlbl)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
                                     
  gtk_widget_set_events (explbl, gtk_widget_get_events (explbl)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
      /* styling background color to black */
    GtkCssProvider* provider = gtk_css_provider_new();
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen = gdk_display_get_default_screen(display);


    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                    global_css, 
                                    -1, NULL);
    g_object_unref(provider);
  
  
  
  
  
  
  gtk_widget_show_all (window);
	
	thbtn_update();
	noirbtn_update();
	upd_lbl_res();
	upd_lbl_shut();
	upd_lbl_exp();
	ifbtn_upd();
	vfbtn_upd();
  
  
}


void check_update(void)
{	/* if (!devbox) */ return 0;           // this dosent work & I dont want it on my dev cam
    if (have_ip) 
	{	gtk_label_set_text(iplbl, "Checking for updates...");
		system("~/swupd.sh");
	}
	else g_timeout_add_seconds(60, G_CALLBACK (check_update), NULL);
}

void save_thalign(void)
{   FILE* pf = fopen("/home/pi/thalign", "w");
	if (pf)
	{	fprintf(pf, "%f\n", therm_off_x);
	    fprintf(pf, "%f\n", therm_off_y);
	    fprintf(pf, "%f\n", therm_rot_rad);
	    fclose(pf);
	}
}

void save_settings(void)
{	FILE* pf = fopen("/home/pi/settings", "w");
	if (pf)
	{	fprintf(pf, "%i\n", ch_mapping);
		fprintf(pf, "%i\n", therm_pal);
		fprintf(pf, "%i\n", shutter);
		fprintf(pf, "%i\n", camres);
		fprintf(pf, "%i\n", flashir);
		fprintf(pf, "%i\n", flashv);
		fprintf(pf, "%i\n", expmode);
		fclose(pf);
	}
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

void load_settings(void)
{	FILE* pf = fopen("/home/pi/settings", "r");
	if (pf)
	{	char buffer[1024];
		fgets(buffer, 1024, pf);	ch_mapping = atoi(buffer);
		fgets(buffer, 1024, pf);	therm_pal = atoi(buffer);
		fgets(buffer, 1024, pf);	shutter   = atoi(buffer);
		fgets(buffer, 1024, pf);	camres    = atoi(buffer);
		fgets(buffer, 1024, pf);	flashir   = atoi(buffer);
		fgets(buffer, 1024, pf);	flashv    = atoi(buffer);
		fgets(buffer, 1024, pf);	expmode   = atoi(buffer);
		fclose(pf);
	}
	
	thbtn_update();
	noirbtn_update();
	upd_lbl_res();
	upd_lbl_shut();
	upd_lbl_exp();
	ifbtn_upd();
	vfbtn_upd();
}

static gboolean
flash_once(void)
{   flash();
    return FALSE;
}

int main (int argc, char **argv)
{   
    int ps = is_process_running("ctrlr");
    // printf("ps: %i\n", ps);
    
    if (ps > 1)
    {   cam_on=0;
        raspistill_end_misery("process already running");
        kill(getpid(),SIGINT);
        return 1;
    }
    
    if (cfileexists("/home/pi/devbox"))
    {   devbox=1;
        // allowvid=1;
    }
    
    int mypid = getpid();
    char cmdbuf[256];
    sprintf(cmdbuf, "sudo renice -n -18 -p %i", mypid);
    system(cmdbuf);
    
    last_flash = time(NULL);
    
/*
    printf("Camera running: %i\n", is_process_running("raspistill"));
    printf("Thermcam running: %i\n", is_process_running("thermcam2"));
    return 0;
*/
  cam_pid = 0;
  pwrr = 0;
  
  ch_mapping = 1;        // CIR
  therm_pal = _THM_FIRE;        // fire
  shutter = -1;         // auto
  camres = 768;         // 1024x768px
  
  flashy = 0;           // not doing flash
  flashir = 1;          // default on
  flashv = 0;           // default off
  cam_on = 1;           // enable raspistill
  expmode = 0;          // day
  
  firlit = fvlit = 0;   // flash IR/visible lit
  
  thsamp = -1;
  
  listfirst = listlen = 0;
  
  load_settings();
  load_thalign();
  last_cam_init = 0;
  
  imp = 0;
  impt = 0;
  
  if (shutter > 999999) shutter = -1;
  
  // printf("My new friends are starting ");
  FILE* fpargs = fopen("/tmp/ctrlargs", "wb");
  // if (fpargs) printf("to look old like me\n");
  
  int i;
  for (i=1; i<argc; i++)
  { printf("%s\n", argv[i]);
  
    if ( strcmp(argv[i], "thc"))       // if NOT equal
    {   fwrite(argv[i], 1, strlen(argv[i]), fpargs);
        fwrite(" ", 1, 1, fpargs);
    }
    
    if (!strcmp(argv[i], "thc"))       // thermal count
    {   thsamp = 0;
    }
    
    if (!strcmp(argv[i], "tm"))        // time lapse
    {   tmlaps = 60;
        if (i < argc)
        {   int tlsec = atoi(argv[i+1]);
            if (tlsec >= 10) tmlaps = tlsec;
        }
        g_timeout_add_seconds(tmlaps, G_CALLBACK (flash), NULL);
        g_timeout_add_seconds(5, G_CALLBACK (flash_once), NULL);
    }
  }
  
  
  
  fclose(fpargs);
  
  argc = 0;
  ctdn2camreinit = 0;
  
  system("gpio mode 4 out");
  system("gpio mode 0 out");
  system("gpio write 4 1");
  system("gpio write 0 1");
  
  system("sudo vcdbg set awb_mode 1");
  
#if _SIMULSHOT
  // if (!devbox)
#endif
  if (!is_process_running("czrun.sh"))
  { system("/bin/bash /home/pi/czrun.sh &");
  }
  
  load_thermal_mult();
  check_disk_usage();
  
  // return 0;
  
  strcpy(slsh_cimg, "");
  
  int status;

    // shared memory for thermcam
    key_t key = ftok("/tmp/shm",70);
    int shmid  = shmget(key, 2048*sizeof(int), 0666 | IPC_CREAT);
    thermdat = (int*)shmat(shmid, (void*)0, 0);
    
    // shared memory id DOESN'T FUCKING WORK
    /*key_t key2 = ftok("/tmp/shm",81);
    int shmid2  = shmget(key2, 8*sizeof(long), 0666 | IPC_CREAT);
    tmdat = (long*)shmat(shmid2, (void*)0, 0);*/
    tmdat = thermdat;
	
  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  
  
  // g_timeout_add_seconds(30, G_CALLBACK (check_update), NULL);
  
    
    char fnth[256], fnhth[256];

    FILE* fnfp = fopen("fnfn.dat", "r");
    if (fnfp)
    {   // fprintf("%s\n%s\n", fnth, fnhth);
        fgets(fnth, 256, fnfp);
        fgets(fnhth, 256, fnfp);
        fclose(fnfp);
        
        list_add(fnth, 0);
        list_add(fnhth, 1);
    }

  return status;
}


