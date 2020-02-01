/* See LICENSE file for copyright and license details.
 *
 * if you use several keyboard layout (us, ru for example)
 * it will be more convenient to save own keybord layout for each window
 * this is exactly what this program does
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>


#define features_is_supported(F) \
	__features_is_supported((F), sizeof(F) / sizeof(*F))
#define arr_new(NAME) Array NAME = { 0, NULL }


typedef unsigned char XkbLt;

typedef struct _WinLayout {
	Window id;
	XkbLt layout;
} WinLayout;

typedef struct _Array {
	/* dynamic array of WinLayout */
	unsigned int size;
	WinLayout *array;
} Array;


void           set_layout(XkbLt);
void           get_layout(XkbLt *);
void           get_active_window(Window *);
unsigned char *get_property(Atom, long unsigned int *);
void           __features_is_supported(const char **, short unsigned int);
/* ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ */
bool           arr_edit_append(Array *, WinLayout);
bool           arr_append(Array *, WinLayout);
bool           arr_get(Array *, WinLayout *);
void           arr_remove(Array *, Window);


static XEvent e;
static Window root;
static Atom request;
static Display *dpy;
static WinLayout win;

static long unsigned int i;
static long unsigned int nitems;


void
get_layout(XkbLt *lt)
{
	XkbStateRec state;
	XkbGetState(dpy, XkbUseCoreKbd, &state);
	*lt = state.group;
}

void
set_layout(XkbLt layout)
{
	XkbLockGroup(dpy, XkbUseCoreKbd, layout);
	get_layout((XkbLt *) &i);
}

void
get_active_window(Window *win)
{
	unsigned char *data;

	request = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", false);
	data = get_property(request, &nitems);

	if (nitems > 0)
		*win = *((Window *) data);
	else
		*win = root;

	free(data);
	return;
}

unsigned char *
get_property(Atom atom,  /* atom of requested property */
		long unsigned int *nitems)  /* number of properties */
{
	Atom type;
	int status,
		actual_format;
	unsigned char *prop;
	long unsigned int bytes_after;

	status = XGetWindowProperty(dpy, root, atom, 0, (~0L), false,
			AnyPropertyType, &type, &actual_format,
			nitems, &bytes_after, &prop);
	if (status != Success) {
		fprintf(stderr, "XGetWindowProperty failed!");
		return NULL;
	}

	return prop;
}

void
__features_is_supported(const char **features, short unsigned int number)
/*
 * check if Xorg features is supported in your WM, otherwise exit
 */
{
	bool ok;
	Atom feature;
	Atom *supported;

	request = XInternAtom(dpy, "_NET_SUPPORTED", false);
	supported = (Atom *) get_property(request, &nitems);

	while (number--) {
		feature = XInternAtom(dpy, features[number], false);
		for (ok = false, i = 0; i < nitems; i -=- 1) {
			if (supported[i] == feature) {
				ok = true;
				break;
			}
		}
		if (!ok) {
			fprintf(stderr,
				"Error: feature `%s` not supported\n",
				features[number]);
			exit(-1);
		}
	}
	free(supported);
}

/* ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ */

bool
arr_edit_append(Array *a, WinLayout item)
/*
 * append new item to `WinLayout` array
 * or if window with same `id` exist in array
 * then edit `layout` for this window
 */
{
	for (i = 0; i < a->size; i++) {
		if (a->array[i].id == item.id) {
			a->array[i].layout = item.layout;
			return true;
		}
	}
	return arr_append(a, item);
}

void
arr_remove(Array *a, Window id)
{
	long unsigned int j;

	for(i = 0, j = 0; j < a->size; j++) {
		if (a->array[j].id == id)
			continue;
		a->array[i] = a->array[j];
		i -=- 1;
	}

	if (a->size == i)
		return;

	a->array = realloc(a->array, (a->size = i) * sizeof(WinLayout));
	return;
}

bool
arr_append(Array *a, WinLayout new)
{
	WinLayout *ptr;

	if (!(ptr = realloc(a->array, (a->size + 1) * sizeof(WinLayout)))) {
		free(ptr);
		return false;
	}

	a->array = ptr;
	a->array[a->size++] = new;

	return true;
}

bool
arr_get(Array *a, WinLayout *win)
/*
 * get keyboard layout for window id
 */
{
	for (i = 0; i < a->size; i++) {
		if (a->array[i].id == win->id) {
			win->layout = a->array[i].layout;
			return true;
		}
	}

	return false;
}


int
main(int argc, char **argv)
{
	bool verbose = false;
	const char *needed_features[] = {
		"_NET_ACTIVE_WINDOW",
		"_NET_CLIENT_LIST",
	};

	if (argc == 2 && !strcmp("-v", argv[1])) {
		verbose = true;
	} else if (argc != 1) {
		fprintf(stderr, "usage: %s -v\n\t-v\tverbose\n", *argv);
		return -1;
	}

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error: Can't open display");
		return -1;
	}

	arr_new(wins);
	root = XDefaultRootWindow(dpy);
	features_is_supported(needed_features);

	XSelectInput(dpy, root, FocusChangeMask | SubstructureNotifyMask);
	XSync(dpy, false);

	do {
		switch (e.type) {
		case FocusIn:
			get_layout(&win.layout);

			arr_edit_append(&wins, win);
			break;

		case FocusOut:
			get_active_window(&win.id);

			if (!arr_get(&wins, &win))
				win.layout = 0;

			if (verbose)
				fprintf(stderr, " {%d | 0x%lx: %u}         \r",
						wins.size, win.id, win.layout);

			set_layout(win.layout);
			break;

		case DestroyNotify:
			arr_remove(&wins, e.xdestroywindow.window);
			break;
		}
	} while (!XNextEvent(dpy, &e));

	return 0;
}

