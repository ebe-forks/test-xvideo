/*
 * Copyright (C) 2007 OpenedHand Ltd
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <assert.h>
#include <unistd.h>

#define LOG_POSITION \
fprintf(stdout, "in (%s) at %s:%d: ", __func__, __FILE__, __LINE__) ;

#define LOG_ERROR(args...) \
LOG_POSITION ; fprintf (stdout, "error: " args)

#define LOG(args...) \
LOG_POSITION ; fprintf (stdout, args)

#define RETURN_IF_FAIL(expr) \
if (!(expr)) {LOG_ERROR("assertion failed: %s", #expr);return;}

#define RETURN_VAL_IF_FAIL(expr, val) \
if (!(expr)) {LOG_ERROR("assertion failed: %s", #expr);return val;}

#define GUID_YUV12_PLANAR 0x32315659 /*YUV 4:2:0 planar*/
#define GUID_YUV16_PLANAR 0x32315659 /*YUV 4:2:2 planar*/

/******************
 * <data types>
 *****************/
enum bool_t {
    FALSE=0,
    TRUE
};

enum yuv_format_t {
    YUV_FORMAT_UNDEF,
    YUV_FORMAT_420_PLANAR,
    YUV_FORMAT_420_INTERLEAVED,
    YUV_FORMAT_422_PLANAR
};

struct options_t {
    enum bool_t display_help ;
    char *display_name ;
    int width ;
    int height ;
    enum yuv_format_t yuv_format ;
    int nb_frames ;
    char *path_to_yuv_file ;
};

/******************
 * </data types>
 *****************/

void display_help (const char *a_prog_name) ;
void options_init (struct options_t *a_options) ;
enum bool_t parse_command_line (int a_argc,
                                char **a_argv,
                                struct options_t *a_options) ;
void run_event_loop (Display *a_display) ;
void do_dispatch_event (const XEvent *a_event) ;
void do_process_expose_event (const XExposeEvent *a_event) ;
void do_process_map_event (const XMapEvent *a_event) ;

enum bool_t compute_yuv_image_size (enum yuv_format_t a_yuv_format,
                                    unsigned a_width,
                                    unsigned a_height,
                                    unsigned *a_len) ;
enum bool_t
read_next_yuv_image_of_size_and_format (FILE *a_input,
                                        unsigned a_width,
                                        unsigned a_height,
                                        enum yuv_format_t a_yuv_format,
                                        char **a_buf,
                                        unsigned *a_len) ;

enum bool_t get_xv_port (Display *a_display,
                         Drawable a_drawable,
                         XvPortID *a_port) ;

enum bool_t get_xv_supported_image_formats (Display *a_display,
                                            XvPortID a_xv_port,
                                            XvImageFormatValues **a_formats,
                                            int *a_nb_formats) ;
enum bool_t lookup_image_format (Display *a_display,
                                 XvPortID a_xv_port,
                                 int a_id,
                                 XvImageFormatValues *a_image_format) ;

enum bool_t push_yuv_to_xvideo (Display *a_display,
                                Window a_window,
                                int a_x,
                                int a_y,
                                int a_width,
                                int a_height) ;

static struct options_t *options=NULL ;
static Window window ;
static XvPortID xv_port=0 ;
static FILE *yuv_input=NULL ;
static char *current_yuv_frame=NULL ;
static char *current_yuv_frame_len=NULL ;

/*************************
 * <yuv stuff>
 * ***********************/

enum bool_t
compute_yuv_image_size (enum yuv_format_t a_yuv_format,
                        unsigned a_width,
                        unsigned a_height,
                        unsigned *a_len)
{
    RETURN_VAL_IF_FAIL (a_len, FALSE) ;

    switch (a_yuv_format) {
        case YUV_FORMAT_420_PLANAR:
        case YUV_FORMAT_420_INTERLEAVED:
            *a_len = 3*a_width*a_height/2 ;
            break ;
        case YUV_FORMAT_422_PLANAR:
            *a_len = 3*a_width*a_height;
            break ;
        default:
            LOG_ERROR ("unsupported format: %d\n", a_yuv_format) ;
            return FALSE ;
    }
    return TRUE ;
}

enum bool_t
read_next_yuv_image_of_size_and_format (FILE *a_input,
                                        unsigned a_width,
                                        unsigned a_height,
                                        enum yuv_format_t a_yuv_format,
                                        char **a_buf,
                                        unsigned *a_len)
{
    unsigned nb_to_read=0, nb_read=0 ;
    char *buf=NULL ;
    enum bool_t is_ok=FALSE ;

    RETURN_VAL_IF_FAIL (a_input, FALSE) ;

    if (!compute_yuv_image_size (a_yuv_format, a_width,
                                 a_height, &nb_to_read)) {
        LOG_ERROR ("failed to compute image size\n") ;
        return FALSE ;
    }
    RETURN_VAL_IF_FAIL (nb_to_read, FALSE) ;

    buf = calloc (nb_to_read, 1) ;
    if (!buf) {
        LOG_ERROR ("failed to allocate buffer\n") ;
        goto out ;
    }

    LOG ("reading image of %d bytes ...\n", nb_to_read) ;
    nb_read = fread (buf, 1, nb_to_read, a_input) ;
    if (nb_read != nb_to_read) {
        LOG_ERROR ("unexpected end of file\n") ;
        goto out ;
    }
    LOG ("image read ok\n") ;

    *a_buf = buf ;
    buf = NULL ;
    *a_len = nb_read ;
    is_ok = TRUE ;

out:
    if (buf) {
        free (buf) ;
    }
    return is_ok ;
}

enum bool_t
get_xv_port (Display *a_display, Drawable a_drawable, XvPortID *a_port)
{
    enum bool_t result=FALSE ;
    XvAdaptorInfo *adaptor_infos=NULL ;
    int nb_adaptors=0, i=0 ;

    RETURN_VAL_IF_FAIL (a_display && a_port, FALSE) ;

    if (XvQueryAdaptors (a_display, a_drawable,
                         &nb_adaptors, &adaptor_infos) != Success) {
        LOG_ERROR ("XvQueryAdaptors failed\n") ;
        goto out ;
    }
    LOG ("got %d adaptors\n", nb_adaptors) ;

    for (i=0; i<nb_adaptors;i++) {
        /*
         * get the first adaptor that supports
         * video input and image puting
         */
        if ((adaptor_infos[i].type & (XvInputMask|XvImageMask))
            == (XvInputMask|XvImageMask)) {
            XvPortID p = 0 ;
            /*get the first port that we can grab*/
            for (p = adaptor_infos[i].base_id ;
                 p < adaptor_infos[i].base_id + adaptor_infos[i].num_ports;
                 p++) {
                if (XvGrabPort (a_display, p, CurrentTime) == Success) {
                    *a_port = p ;
                    result = TRUE ;
                    goto out ;
                } else {
                    LOG_ERROR ("failed to grab port: %d\n", p) ;
                }
            }
        }
    }

out:
    if (adaptor_infos) {
        XvFreeAdaptorInfo (adaptor_infos) ;
        adaptor_infos = NULL ;
    }
    return result ;
}

enum bool_t
get_xv_supported_image_formats (Display *a_display,
                                XvPortID a_xv_port,
                                XvImageFormatValues **a_formats,
                                int *a_nb_formats)
{
    XvImageFormatValues *image_formats=NULL ;
    int nb_formats=0, i=0 ;

    image_formats = XvListImageFormats (a_display, a_xv_port, &nb_formats) ;
    if (!image_formats) {
        return FALSE ;
    }
    LOG_ERROR ("XServer supports %d formats\n", nb_formats) ;
    for (i=0 ; i < nb_formats ; i++) {
        LOG ("format id: %#x", image_formats[i]) ;
    }
    XFree (image_formats) ;
    return TRUE ;
}

enum bool_t
lookup_image_format (Display *a_display,
                     XvPortID a_xv_port,
                     int a_format,
                     XvImageFormatValues *a_image_format)
{
    XvImageFormatValues *image_formats=NULL ;
    int nb_formats=0, i=0 ;
    enum bool_t is_ok=FALSE ;

    RETURN_VAL_IF_FAIL (a_display && a_image_format, FALSE) ;

    image_formats = XvListImageFormats (a_display, a_xv_port, &nb_formats) ;
    if (!image_formats) {
        goto out ;
    }
    for (i=0 ; i < nb_formats ; i++) {
        if (image_formats[i].id == a_format) {
            memcpy (a_image_format,
                    &image_formats[i],
                    sizeof (XvImageFormatValues)) ;
            is_ok = TRUE ;
            break ;
        }
    }

out:
    if (image_formats) {
        XFree (image_formats) ;
    }
    return is_ok ;
}

enum bool_t
push_yuv_to_xvideo (Display *a_display,
                    Window a_window,
                    int a_x,
                    int a_y,
                    int a_width,
                    int a_height)
{
    enum bool_t is_ok = FALSE ;
    XvImageFormatValues image_format ;
    XvImage *xv_image=NULL ;
    GC gc=0 ;
    XGCValues gc_values;
    char *yuv_buf=NULL ;
    unsigned yuv_buf_len=0 ;

    if (!get_xv_port (a_display, (Drawable)a_window, &xv_port)) {
        LOG_ERROR ("could not get xv port\n") ;
        goto out ;
    }
    LOG ("Got xv port: %d\n", xv_port) ;

    if (!lookup_image_format (a_display, xv_port,
                              GUID_YUV12_PLANAR, &image_format)) {
        LOG_ERROR ("yuv12planar format not supported by xserver\n") ;
        goto out ;
    }

    if (!read_next_yuv_image_of_size_and_format (yuv_input,
                                                 a_width,
                                                 a_height,
                                                 YUV_FORMAT_420_PLANAR,
                                                 &yuv_buf,
                                                 &yuv_buf_len)) {
        LOG_ERROR ("could not ready frame from yuv file\n") ;
        goto out ;
    }
    xv_image = (XvImage*) XvCreateImage (a_display,
                                         xv_port, image_format.id,
                                         yuv_buf, a_width, a_height) ;
    if (!xv_image) {
        LOG_ERROR ("failed to create image\n") ;
        goto out ;
    }

    gc = XCreateGC (a_display, a_window, 0L, &gc_values) ;
    if (!gc) {
        LOG_ERROR ("failed to create gc \n") ;
        goto out ;
    }

    XvPutImage (a_display, xv_port, a_window, gc, xv_image,
                a_x, a_y, a_width, a_height,
                a_x, a_y, a_width, a_height) ;

    is_ok = TRUE ;

out:
    if (xv_image) {
        XFree (xv_image) ;
    }
    if (yuv_buf) {
        free (yuv_buf) ;
    }
    return is_ok ;
}

/*************************
 * </yuv stuff>
 * ***********************/

/**************************
 * <x11 stuff>
 * ************************/

void
run_event_loop (Display *a_display)
{
    XEvent event ;

    RETURN_IF_FAIL (a_display) ;

    for (;;) {
        XNextEvent (a_display, &event) ;
        do_dispatch_event ((XEvent*)&event) ;
    }
}

void
do_dispatch_event (const XEvent *a_event)
{
    RETURN_IF_FAIL (a_event) ;

    switch (a_event->type) {
        case Expose:
            do_process_expose_event ((XExposeEvent*)a_event) ;
            break ;
        case MapNotify:
            do_process_map_event ((XMapEvent*)a_event) ;
            break ;
        default:
            break ;
    }
}

void
do_process_expose_event (const XExposeEvent *a_event)
{
    LOG ("mark\n") ;
}

void
do_process_map_event (const XMapEvent *a_event)
{

    RETURN_IF_FAIL (a_event && options) ;

    if (a_event->window != window)
        return ;

    if (!push_yuv_to_xvideo (a_event->display, a_event->window,
                             0, 0, options->width, options->height)) {
        LOG_ERROR ("failed to push yuv to xvideo\n") ;
        return ;
    }
    LOG_ERROR ("pushed yuv to xvideo ok\n") ;
}

/**************************
 * </x11 stuff>
 * ************************/

/*****************************************
 * <display help, command line parsing>
 * ***************************************/
void
display_help (const char *a_prog_name)
{
    if (!a_prog_name)
        return ;

    fprintf (stderr,
             "usage: %s [options] <path-to-yuv-file>\n", a_prog_name) ;
    fprintf (stderr,
             "where options can be: \n"
              "--help               display this help\n"
              "--display            X11 display\n"
              "--width <width>      width of the frame\n"
              "--height <height>    height of the frame\n"
              "--nb-frames <nb>     read nb frames from yuv file"
                                                      " (all by default)\n"
              "--yuv420planar       input yuv format is 420 planar (default)\n"
              "--yuv420interleaved  input yuv format is 420 interleaved\n"
              "--yuv422planar       input yuv format is 422 interleaved\n") ;
}

void
options_init (struct options_t *a_options)
{
    if (!a_options)
        return ;
    memset (a_options, 0, sizeof (struct options_t)) ;
    a_options->width = 10 ;
    a_options->height = 10 ;
    a_options->yuv_format = YUV_FORMAT_420_PLANAR ;
}

void
options_free_members (struct options_t *a_opts)
{
    if (!a_opts)
        return ;
    if (a_opts->path_to_yuv_file) {
        free (a_opts->path_to_yuv_file) ;
        a_opts->path_to_yuv_file = NULL ;
    }
    if (a_opts->display_name) {
        free (a_opts->display_name) ;
        a_opts->display_name = NULL ;
    }
}


enum bool_t
parse_command_line (int a_argc, char **a_argv, struct options_t *a_options)
{
    int i=0 ;

    if (!a_argv || !a_options)
        return -1;

    for (i=1 ; i < a_argc ; i++) {
        if (a_argv[i][0] != '-')
            break ;

        if (!strcmp (a_argv[i], "--help") || !strcmp (a_argv[i], "-h")) {
            a_options->display_help = TRUE ;
            return TRUE ;
        }
        if (!strcmp (a_argv[i], "--display")) {
            if (i >= a_argc || a_argv[i+1][0] == '-') {
                LOG_ERROR ("please, give an argument to  --display\n") ;
                a_options->display_help = TRUE ;
                return FALSE ;
            }
            a_options->display_name = strdup (a_argv[i+1]) ;
            i++ ;
        } else if (!strcmp (a_argv[i], "--width")) {
            if (i >= a_argc || a_argv[i+1][0] == '-') {
                LOG_ERROR ("please, give an size argument to  --width\n") ;
                a_options->display_help = TRUE ;
                return FALSE ;
            }
            a_options->width = atoi (a_argv[i+1]) ;
            i++ ;
        } else if (!strcmp (a_argv[i], "--height")) {
            if (i >= a_argc || a_argv[i+1][0] == '-') {
                LOG_ERROR ("please, give an size argument to --height\n") ;
                a_options->display_help = TRUE ;
                return FALSE ;
            }
            a_options->height = atoi (a_argv[i+1]) ;
            i++ ;
        } else if (!strcmp (a_argv[i], "--nb-frames")) {
            if (i >= a_argc || a_argv[i+1][0] == '-') {
                LOG_ERROR ("please, give an size argument to  --nb-frames\n") ;
                a_options->display_help = TRUE ;
                return FALSE ;
            }
            a_options->nb_frames = atoi (a_argv[i+1]) ;
            i++ ;
        } else if (!strcmp (a_argv[i], "--yuv420planar")) {
            a_options->yuv_format = YUV_FORMAT_420_PLANAR ;
        } else if (!strcmp (a_argv[i], "--yuv420interleaved")) {
            a_options->yuv_format = YUV_FORMAT_420_INTERLEAVED ;
        } else if (!strcmp (a_argv[i], "--yuv422planar")) {
            a_options->yuv_format = YUV_FORMAT_422_PLANAR ;
        } else {
            LOG_ERROR ("unknown option: %s\n", a_argv[i]) ;
            a_options->display_help = TRUE ;
            return FALSE ;
        }
    }
    if (i >= a_argc || a_argv[i][0] == '-') {
        LOG_ERROR ("you must give the path to yuv file\n") ;
        return FALSE ;
    }
    a_options->path_to_yuv_file = strdup (a_argv[i]);
    return TRUE ;
}
/*****************************************
 * </display help, command line parsing>
 * ***************************************/

int
main (int argc, char **argv)
{
    struct options_t opts ;
    int result=1;
    Display *display=NULL ;
    int black_color=0, white_color=0,
        xv_major=0, xv_first_event=0,
        xv_first_error=0 ;
    char *display_name ;

    options_init (&opts) ;
    if (!parse_command_line (argc, argv, &opts) || opts.display_help) {
        display_help (argv[0]) ;
        goto out ;
    }
    options = &opts ;

    /*open yuv input file*/
    yuv_input = fopen (opts.path_to_yuv_file, "r") ;
    if (!yuv_input) {
        LOG_ERROR ("could not open file '%s'\n", opts.path_to_yuv_file) ;
        goto out ;
    }

    /*if user gave no display, get into $DISPLAY*/
    if (!opts.display_name && getenv ("DISPLAY")) {
        opts.display_name = strdup (getenv ("DISPLAY")) ;
    }

    display = XOpenDisplay (opts.display_name) ;
    if (!display) {
        LOG_ERROR ("Could not open display\n") ;
        goto out ;
    }

    /*make sure the xserver has the xvideo extension*/
    if (XQueryExtension (display,
                         "XVideo",
                         &xv_major,
                         &xv_first_event,
                         &xv_first_error) != True) {
        LOG_ERROR ("XServer does not support the XVideo extention\n") ;
        goto out ;
    }
    LOG ("XServer supports XVideo extension. cool!\n") ;

    /*create a window*/
    black_color = BlackPixel (display, DefaultScreen (display)) ;
    window = XCreateSimpleWindow (display,
                                  DefaultRootWindow (display),
                                  0, 0, 320, 240, 0,
                                  black_color, black_color) ;

    /*select events we want on that window*/
    XSelectInput (display, window, ExposureMask|StructureNotifyMask) ;

    /*map the window*/
    XMapWindow (display, window) ;
    XFlush (display) ;

    run_event_loop (display) ;
    result = 1 ;

out:
    options_free_members (&opts) ;
    return result;
}
