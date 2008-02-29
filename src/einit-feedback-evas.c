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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <einit/event.h>

#include <einit/configuration-static.h>

#include <einit/einit.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>

static int fb;
struct fb_var_screeninfo fb_var;
Evas_Object *   base_rect;

void event_handler_feedback_notice (struct einit_event *ev) {
 if (ev->string)
  fprintf (stdout, "[notice] %i: %s\n", ev->flag, ev->string);
}

void event_handler_update_module_status (struct einit_event *ev) {
 if (ev->string)
  fprintf (stdout, "[%s] %s\n", ev->rid, ev->string);
}

void event_handler_update_service_enabled (struct einit_event *ev) {
 fprintf (stdout, "[%s] enabled\n", ev->string);
}

void event_handler_update_service_disabled (struct einit_event *ev) {
 fprintf (stdout, "[%s] disabled\n", ev->string);
}

void help (char **argv) {
 printf ("Usage: %s [options]\n\n"
         "Options:\n"
         " -h, --help         This Message\n", argv[0]);
}

void key_down(void *data, Evas *e, Evas_Object *obj, void *event_info) {
        Evas_Event_Key_Down *ev;
        ev = (Evas_Event_Key_Down *)event_info;
        if (strmatch(ev->keyname,"q")) {
        	ecore_main_loop_quit();
        	exit(EXIT_SUCCESS);
        }
}

static int main_signal_exit(void *data, int ev_type, void *ev)
{
   ecore_main_loop_quit();
   return 1;
}

int main(int argc, char **argv, char **env) {
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
 
 if (!ecore_init()) return EXIT_FAILURE;
 if (!ecore_evas_init()) return EXIT_FAILURE;
 
 if (-1 == (fb = open("/dev/fb0",O_RDWR /* O_WRONLY */))) {
  fprintf(stderr,"open /dev/fb0: %s\n",strerror(errno));
  exit(EXIT_FAILURE);
 }
 if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_var)) {
  perror("ioctl FBIOGET_VSCREENINFO");
  exit(EXIT_FAILURE);
 } 
 close(fb);
 
 Ecore_Evas  *ecore_evas = NULL;
 //ecore_evas = ecore_evas_fb_new(NULL, 0, fb_var.xres, fb_var.yres);
 ecore_evas = ecore_evas_software_x11_new(NULL, 0, 0, 0, fb_var.xres, fb_var.yres);
 //ecore_evas = ecore_evas_sdl_new(NULL, fb_var.xres, fb_var.yres, 0, 1, 0, 1);
 if (!ecore_evas) return EXIT_FAILURE;

 ecore_evas_title_set(ecore_evas, "eINIT Evas Feedback Daemon");
 ecore_evas_name_class_set(ecore_evas, "einit-feedback-eval", "einit-feedback-eval");
 ecore_evas_show(ecore_evas);

 Evas *evas = NULL;
 evas = ecore_evas_get(ecore_evas);

 ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, main_signal_exit, NULL);
 base_rect = evas_object_gradient_add(evas);
 evas_object_gradient_fill_angle_set(base_rect, 0);
 evas_object_gradient_fill_spread_set(base_rect, 1);
 evas_object_gradient_fill_set(base_rect, 0, 0, fb_var.xres, fb_var.yres);
 evas_object_gradient_clear(base_rect);
 evas_object_gradient_color_stop_add(base_rect, 52, 101, 164, 255, 2);
 evas_object_gradient_color_stop_add(base_rect, 211, 215, 207, 255, 2);
 evas_object_resize(base_rect, (double)fb_var.xres, (double)fb_var.yres);
 evas_object_image_fill_set(base_rect, 0, 0, fb_var.xres, fb_var.yres);
 evas_object_show(base_rect);
 evas_object_event_callback_add(base_rect,EVAS_CALLBACK_KEY_DOWN, key_down, NULL);   

 if (!einit_connect(&argc, argv)) {
  perror ("Could not connect to eINIT");
  sleep (1);
  if (!einit_connect(&argc, argv)) {
   perror ("Could not connect to eINIT, giving up");
   exit (EXIT_FAILURE);
  }
 }

 evas_object_gradient_add(evas);
 
 ethread_spawn_detached((void *(*)(void *))ecore_main_loop_begin,NULL);
 
 event_listen (einit_feedback_notice, event_handler_feedback_notice);
 event_listen (einit_feedback_module_status, event_handler_update_module_status);
 event_listen (einit_core_service_enabled, event_handler_update_service_enabled);
 event_listen (einit_core_service_disabled, event_handler_update_service_disabled);

 //event_listen (einit_event_subsystem_any, (void (*)(struct einit_event *))ecore_main_loop_iterate);

 einit_event_loop();
 
 einit_disconnect();

 ecore_evas_shutdown();
 ecore_shutdown();

 return 0;
}
