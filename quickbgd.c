#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include <Imlib2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#define MAX_IMAGES 32


typedef enum { Full, Fill, Center, Tile, Xtend, Cover } ImageMode;
typedef struct {
  int r, g, b, a;
} Color;


int screen = 0;
Display *display;
int pixc = 0;
Pixmap pixmaps[MAX_IMAGES];


// Adapted from fluxbox' bsetroot
int set_root_atoms(Pixmap pixmap) {
	Atom atom_root, atom_eroot, type;
	unsigned char *data_root, *data_eroot;
	int format;
	unsigned long length, after;

	atom_root = XInternAtom(display, "_XROOTMAP_ID", True);
	atom_eroot = XInternAtom(display, "ESETROOT_PMAP_ID", True);

	// doing this to clean up after old background
	if (atom_root != None && atom_eroot != None) {
		XGetWindowProperty(display, RootWindow(display, screen), atom_root, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_root);

		if (type == XA_PIXMAP) {
			XGetWindowProperty(display, RootWindow(display, screen), atom_eroot, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_eroot);

			if (data_root && data_eroot && type == XA_PIXMAP && *((Pixmap *) data_root) == *((Pixmap *) data_eroot))
				XKillClient(display, *((Pixmap *) data_root));
		}
	}

	atom_root = XInternAtom(display, "_XROOTPMAP_ID", False);
	atom_eroot = XInternAtom(display, "ESETROOT_PMAP_ID", False);

	if (atom_root == None || atom_eroot == None)
		return 0;

	// setting new background atoms
	XChangeProperty(display, RootWindow(display, screen), atom_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &pixmap, 1);
	XChangeProperty(display, RootWindow(display, screen), atom_eroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &pixmap, 1);

	return 1;
}

int load_image(ImageMode mode, const char *arg, Imlib_Image rootimg, XineramaScreenInfo *outputs, int noutputs) {
	int imgW, imgH;
	Imlib_Image buffer = imlib_load_image(arg);

	if (!buffer)
		return 0;

	imlib_context_set_image(buffer);
	imgW = imlib_image_get_width();
	imgH = imlib_image_get_height();

	imlib_context_set_image(rootimg);

	for (int i = 0; i < noutputs; i++) {
		XineramaScreenInfo o = outputs[i];
		//printf("output %d: size(%d, %d) pos(%d, %d)\n", i, o.width, o.height, o.x_org, o.y_org);

		if (mode == Fill) {
			imlib_blend_image_onto_image(buffer, 0, 0, 0, imgW, imgH, o.x_org, o.y_org, o.width, o.height);
		} else if ((mode == Full) || (mode == Xtend) || (mode == Cover)) {
			double aspect = ((double) o.width) / imgW;
			if (((int) (imgH * aspect) > o.height) != /*xor*/ (mode == Cover))
				aspect = (double) o.height / (double) imgH;

			int top = (o.height - (int) (imgH * aspect)) / 2;
			int left = (o.width - (int) (imgW * aspect)) / 2;

			imlib_blend_image_onto_image(buffer, 0, 0, 0, imgW, imgH, o.x_org + left, o.y_org + top, (int) (imgW * aspect), (int) (imgH * aspect));

			if (mode == Xtend) {
				int w;

				if (left > 0) {
					int right = left - 1 + (int) (imgW * aspect);
					/* check only the right border - left is int divided so the right border is larger */
					for (w = 1; right + w < o.width; w <<= 1) {
						imlib_image_copy_rect(o.x_org + left + 1 - w, o.y_org, w, o.height, o.x_org + left + 1 - w - w, o.y_org);
						imlib_image_copy_rect(o.x_org + right, o.y_org, w, o.height, o.x_org + right + w, o.y_org);
					}
				}

				if (top > 0) {
					int bottom = top - 1 + (int) (imgH * aspect);
					for (w = 1; (bottom + w < o.height); w <<= 1) {
						imlib_image_copy_rect(o.x_org, o.y_org + top + 1 - w, o.width, w, o.x_org, o.y_org + top + 1 - w - w);
						imlib_image_copy_rect(o.x_org, o.y_org + bottom, o.width, w, o.x_org, o.y_org + bottom + w);
					}
				}
			}
		} else {  // Center || Tile
			int left = (o.width - imgW) / 2;
			int top = (o.height - imgH) / 2;

			if (mode == Tile) {
				int x, y;
				for (; left > 0; left -= imgW);
				for (; top > 0; top -= imgH);

				for (x = left; x < o.width; x += imgW)
					for (y = top; y < o.height; y += imgH)
						imlib_blend_image_onto_image(buffer, 0, 0, 0, imgW, imgH, o.x_org + x, o.y_org + y, imgW, imgH);
			} else {
				imlib_blend_image_onto_image(buffer, 0, 0, 0, imgW, imgH, o.x_org + left, o.y_org + top, imgW, imgH);
			}
		}
	}

	imlib_context_set_image(buffer);
	imlib_free_image();

	imlib_context_set_image(rootimg);

	return 1;
}

void set_background(int index) {
	Pixmap pixmap = pixmaps[index];
	if(set_root_atoms(pixmap) == 0)
		fprintf(stderr, "Couldn't create atoms...\n");

	XKillClient(display, AllTemporary);
	XSetCloseDownMode(display, RetainTemporary);

	XSetWindowBackgroundPixmap(display, RootWindow(display, screen), pixmap);
	XClearWindow(display, RootWindow(display, screen));

	XFlush(display);
	XSync(display, False);
}

void loop_message_queue(){
	char buffer[1];

	/* Define message queue attributes */
	struct mq_attr attr = {
		.mq_flags = 0,
		.mq_maxmsg = 8,
		.mq_msgsize = 1,
		.mq_curmsgs = 0
	};

	/* Open the message queue */
	mqd_t mq = mq_open("/quickbgd", O_CREAT | O_RDONLY, 0644, &attr);
	if(mq == (mqd_t) -1){
		perror("Error creating message queue");
		exit(1);
	}

	while(1){
		ssize_t bytes_read = mq_receive(mq, buffer, 1, NULL);
		if(bytes_read != 1)
			continue;

		/* Convert ASCII number to decimal */
		int n = buffer[0] - '0';
		if(n >= 1 && n <= pixc){
			set_background(n - 1);
		} else {
			set_background(0);
		}
	}
}

int main(int argc, char **argv) {
	if(argc < 2){
		fprintf(stderr, "At least one image is required.\n");
		return 1;
	}
	if(argc - 1 > MAX_IMAGES){
		fprintf(stderr, "Warning: surpassed maximum images (%d)", MAX_IMAGES);
	}


	/* Open display */
	display = XOpenDisplay(NULL);
	if(!display){
		fprintf(stderr, "Cannot open X display!\n");
		return 1;
	}
	int noutputs = 0;
	XineramaScreenInfo* outputs = XineramaQueryScreens(display, &noutputs);

	/* Initialize Imlib */
	Visual* vis = DefaultVisual(display, screen);
	Colormap cm = DefaultColormap(display, screen);
	int width   = DisplayWidth(display, screen);
	int height  = DisplayHeight(display, screen);
	int depth   = DefaultDepth(display, screen);
	Imlib_Context* context = imlib_context_new();
	imlib_context_push(context);
	imlib_context_set_display(display);
	imlib_context_set_visual(vis);
	imlib_context_set_colormap(cm);
	imlib_context_set_color_range(imlib_create_color_range());
	imlib_context_set_color(0, 0, 0, 255);
	imlib_context_set_dither(1);
	imlib_context_set_blend(1);

	/* Load all images */
	for(int i = 1; i < argc; i++){
		Imlib_Image image = imlib_create_image(width, height);
		Pixmap pixmap = XCreatePixmap(display, RootWindow(display, screen), width, height, depth);

		imlib_context_set_drawable(pixmap);
		imlib_context_set_image(image);
		imlib_image_fill_rectangle(0, 0, width, height);

		if(load_image(Cover, argv[i], image, outputs, noutputs) == 0){
			fprintf(stderr, "Bad image (%s)\n", argv[i]);
			exit(1);
		} else {
			printf("Loaded image: %s\n", argv[i]);
		}

		imlib_render_image_on_drawable(0, 0);
		imlib_free_image();
		pixmaps[pixc++] = pixmap;
		if(pixc == MAX_IMAGES)
			break;
	}
	imlib_free_color_range();
	imlib_context_pop();
	imlib_context_free(context);

	loop_message_queue();

	return 0;
}
