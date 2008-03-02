/*
 *  einit-feedback-evas.c
 *  einit
 *
 *  Created by Ryan Hope on 02/28/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/vt.h>

#include <einit/event.h>

#include <einit/configuration-static.h>

#include <einit/einit.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_Fb.h>
#include <Evas_Engine_FB.h>

#define SLOW 5.0

struct fb_var_screeninfo fb_var;

int WIDTH = 800;
int HEIGHT = 600;

static int  vtno;
int  LinuxConsoleFd;
static int  activeVT;

Evas_Object *base_rect;
Ecore_Evas  *ecore_evas;
Evas_Event_Key_Down *ev;
Evas *evas;
struct stree *msg_tree = NULL;

Evas_Object *create_text_object(char *msg) {
	Evas_Object *o;
	o = evas_object_text_add(evas);
	evas_object_text_font_set(o, "Vera-Bold", 20);
	evas_object_text_text_set(o, msg);
	evas_object_color_set(o, 0, 0, 0, 255);
	evas_object_show(o);
	return o;
}

void event_handler_feedback_notice (struct einit_event *ev) {
	if (ev->string) {
		char msg[BUFFERSIZE];
		snprintf (msg, BUFFERSIZE, "[notice] %i: %s\n", ev->flag, ev->string);
		Evas_Object *o = create_text_object(msg);
	}
}

void event_handler_update_module_status (struct einit_event *ev) {
	if (ev->string) {
		char msg[BUFFERSIZE];
		snprintf (msg, BUFFERSIZE, "[%s] %s\n", ev->rid, ev->string);
		Evas_Object *o = create_text_object(msg);
	}
}

void event_handler_update_service_enabled (struct einit_event *ev) {
	char msg[BUFFERSIZE];
	snprintf (msg, BUFFERSIZE, "[%s] enabled\n", ev->string);
	Evas_Object *o = create_text_object(msg);
}

void event_handler_update_service_disabled (struct einit_event *ev) {
	char msg[BUFFERSIZE];
	snprintf (msg, BUFFERSIZE, "[%s] disabled\n", ev->string);
	Evas_Object *o = create_text_object(msg);
}

void help (char **argv) {
	printf ("Usage: %s [options]\n\n"
         "Options:\n"
         " -h, --help         This Message\n", argv[0]);
}

static int really_quit() {
	ecore_main_loop_quit();
	return 1;
}

void key_down(void *data, Evas *e, Evas_Object *obj, void *event_info) {
	ev = (Evas_Event_Key_Down *)event_info;
	evas_object_gradient_color_stop_add(base_rect, 92, 21, 211, 255, 2);
	evas_object_show(base_rect);
	if (strmatch(ev->keyname,"q")) {
		/* 
		 * Try to switch back to original console
		 */

		ecore_timer_add(1.0, really_quit, NULL);
	}
}

static int main_signal_exit(void *data, int ev_type, void *ev) {
	ecore_timer_add(1.0, really_quit, NULL);
	return 1;
}

void check_chown (char *file) {
    struct stat	    st;
    __uid_t	    u;
    __gid_t	    g;
    if (stat (file, &st) < 0)
	return;
    u = getuid ();
    g = getgid ();
    if (st.st_uid != u || st.st_gid != g)
	chown (file, u, g);
}


int main(int argc, char **argv, char **env) {
	/*
	 * Parse command line options
	 */	
	int i = 1;
	for (; argv[i]; i++) {
		if (strmatch (argv[i], "-h") || strmatch (argv[i], "--help")) {
			help(argv);
			return EXIT_SUCCESS;
		} else {
			help(argv);
			return EXIT_FAILURE;
		}
	}
	
	/*
	 * Connect to einit
	 */
	if (!einit_connect(&argc, argv)) {
	  perror ("Could not connect to eINIT");
	  sleep (1);
	  if (!einit_connect(&argc, argv)) {
	   perror ("Could not connect to eINIT, giving up");
	   exit (EXIT_FAILURE);
	  }
	 }

	/*
	 * Get framebuffer resolution
	 */
	int fb;
	if (-1 == (fb = open("/dev/fb0",O_RDWR /* O_WRONLY */))) {
		fprintf(stderr,"open /dev/fb0: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_var)) {
		perror("ioctl FBIOGET_VSCREENINFO");
		exit(EXIT_FAILURE);
	} 
	close(fb);
	WIDTH = fb_var.xres;
	HEIGHT = fb_var.yres;
	
	/*
	 * Get current tty
	 */
    int fd;
    char vtname[11];
    struct vt_stat vts;
    struct stat	statb;
    LinuxConsoleFd = -1;
    if ((fd = open("/dev/tty0",O_WRONLY,0)) < 0) return EXIT_FAILURE;
    if ((ioctl(fd, VT_OPENQRY, &vtno) < 0) || (vtno == -1)) close(fd);
    sprintf(vtname,"/dev/tty%d",vtno);
    if ((LinuxConsoleFd = open(vtname, O_RDWR|O_NDELAY, 0)) < 0) return EXIT_FAILURE;
    check_chown(vtname);
    check_chown("/dev/tty0");
    if (ioctl(LinuxConsoleFd, VT_GETSTATE, &vts) == 0) activeVT = vts.v_active;
    printf("%d\n",activeVT);
    sleep(5);
	
	/*
	 * Init ecore-evas
	 */
	if (!ecore_init()) return EXIT_FAILURE;
	if (!evas_init()) return EXIT_FAILURE;
	if (!ecore_evas_init()) return EXIT_FAILURE;
	if (!ecore_fb_init(NULL)) return EXIT_FAILURE;

	/*
	 * Setup framebuffer shit
	 */
	ecore_evas = ecore_evas_fb_new(NULL, 0, WIDTH, HEIGHT);
	if (!ecore_evas) return EXIT_FAILURE;
	
	evas = ecore_evas_get(ecore_evas);
	Evas_Engine_Info_FB *einfo;
	evas_output_method_set(evas, evas_render_method_lookup("fb"));
	einfo = (Evas_Engine_Info_FB *)evas_engine_info_get(evas);
	if (!einfo) {
		printf("Evas does not support the FB Engine\n");
		return 0;
	}
	einfo->info.virtual_terminal = 0;
	einfo->info.device_number = 0;
	einfo->info.device_number = 0;
	einfo->info.refresh = 0;
	einfo->info.rotation = 0;
	evas_engine_info_set(evas, (Evas_Engine_Info *) einfo);
	
	/*
	 * Setup evas canvas
	 */
	ecore_evas_title_set(ecore_evas, "eINIT Evas Feedback Daemon");
	ecore_evas_name_class_set(ecore_evas, "einit-feedback-eval", "einit-feedback-eval");
	ecore_evas_show(ecore_evas);

	base_rect = evas_object_gradient_add(evas);
	evas_object_gradient_fill_angle_set(base_rect, 0);
	evas_object_gradient_fill_spread_set(base_rect, 1);
	evas_object_gradient_fill_set(base_rect, 0, 0, WIDTH, HEIGHT);
	evas_object_gradient_clear(base_rect);
	evas_object_gradient_color_stop_add(base_rect, 52, 101, 164, 255, 2);
	evas_object_gradient_color_stop_add(base_rect, 211, 215, 207, 255, 2);
	evas_object_resize(base_rect, (double)WIDTH, (double)HEIGHT);
	evas_object_image_fill_set(base_rect, 0, 0, WIDTH, HEIGHT);
	evas_object_show(base_rect);
	evas_object_focus_set(base_rect,1);    
	evas_object_gradient_add(evas);

	/*
	 * Register ecore-evas events
	 */
	ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, main_signal_exit, NULL);
	evas_object_event_callback_add(base_rect,EVAS_CALLBACK_KEY_DOWN, key_down, NULL);
 
	/*
	 * Spawn ecore main loop in own thread
	 */
	ethread_spawn_detached((void *(*)(void *))ecore_main_loop_begin,NULL);
 
	/*
	 * Regester event listeners
	 */
	event_listen (einit_feedback_notice, event_handler_feedback_notice);
	event_listen (einit_feedback_module_status, event_handler_update_module_status);
	event_listen (einit_core_service_enabled, event_handler_update_service_enabled);
	event_listen (einit_core_service_disabled, event_handler_update_service_disabled);
	
	/*
	 * Start einit event loop
	 */
	einit_event_loop();
 
	/*
	 * Shutdown gracefully
	 */
	einit_disconnect();
	ecore_evas_shutdown();
	ecore_shutdown();
	evas_shutdown(); 
	ecore_main_loop_quit();
	
	really_quit();
}
