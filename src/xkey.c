#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <ltlib.h>
#include "logger.h"
#include "xkey.h"
#include "cfg.h"

#define MAX(a, b)	((a) > (b) ? (a) : (b))

static KeyCode bind_key(Display *dpy, const char *keyname, const char *opname);
static void *thread_start(void *arg);
static void cleanup(void *arg);
static void handle_xevent(Display *dpy, XEvent *xev);
static int xerr(Display *dpy, XErrorEvent *err);

static KeyCode pause_keycode, recenter_keycode;
static pthread_t thread;
static int pfd[2];	/* a self-pipe for reliable termination of the X thread */

/* used to provide meaningful error messages in the event of grab failure */
static const char *xerr_key, *xerr_op;

void init_keyb(void)
{
	int res = pthread_create(&thread, 0, thread_start, 0);

	if(res != 0) {
		logmsg("failed to spawn keybindings handler thread: %s\n", strerror(res));
	}

	if(pipe(pfd) == -1) {
		pfd[0] = -1;	/* will fallback to pthread_cancel */
	}
}

static void *thread_start(void *arg)
{
	int xsock;
	Display *dpy;
	Window win, root;
	struct config *cfg;

	logmsg("setting up X keybindings\n");

	if(!(dpy = XOpenDisplay(0))) {
		const char *dpyenv = getenv("DISPLAY");
		logmsg("failed to connect to the X server (%s)\n", dpyenv ? dpyenv : "DISPLAY not set");
		pthread_exit(0);
	}
	root = DefaultRootWindow(dpy);
	xsock = ConnectionNumber(dpy);

	win = XCreateWindow(dpy, root, 0, 0, 8, 8, 0, CopyFromParent, InputOnly, CopyFromParent, 0, 0);
	if(!win) {
		logmsg("failed to create X window for input processing\n");
		pthread_exit(0);
	}
	XSelectInput(dpy, win, KeyPressMask);

	cfg = get_config();

	recenter_keycode = bind_key(dpy, cfg->recenter_key, "recenter");
	if(!recenter_keycode) {
		bind_key(dpy, "Scroll_Lock", "recenter");
	}

	pause_keycode = bind_key(dpy, cfg->pause_key, "pause");
	if(!pause_keycode) {
		bind_key(dpy, "Pause", "pause");
	}

	pthread_cleanup_push(cleanup, (void*)dpy);

	for(;;) {
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(xsock, &fds);

		if(pfd[0] != -1) {
			FD_SET(pfd[0], &fds);
		}

		while(select(MAX(xsock, pfd[0]) + 1, &fds, 0, 0, 0) == -1 && errno == EINTR);

		if(FD_ISSET(xsock, &fds)) {
			while(XPending(dpy)) {
				XEvent xev;
				XNextEvent(dpy, &xev);
				handle_xevent(dpy, &xev);
			}
		}

		if(pfd[0] != -1 && FD_ISSET(pfd[0], &fds)) {
			char c;
			if(read(pfd[0], &c, 1) == 1 && c == 'q') {
				break;
			}
		}
	}

	pthread_cleanup_pop(1);
	return 0;
}

static void cleanup(void *arg)
{
	Display *dpy = arg;

	if(dpy) {
		Window root = DefaultRootWindow(dpy);
		XUngrabKey(dpy, XK_Scroll_Lock, AnyModifier, root);
		XUngrabKey(dpy, XK_Pause, AnyModifier, root);
		XCloseDisplay(dpy);
		dpy = 0;
	}
}

static KeyCode bind_key(Display *dpy, const char *keyname, const char *opname)
{
	KeySym sym;
	KeyCode code;
	int (*xerr_prev)();
	Window root = DefaultRootWindow(dpy);

	logmsg("binding key \"%s\" to %s\n", keyname, opname);

	if((sym = XStringToKeysym(keyname)) == NoSymbol) {
		logmsg("failed to find a key named \"%s\"\n", keyname);
		return 0;
	}
	code = XKeysymToKeycode(dpy, sym);

	/* Attempt to bind keys. Will log an error message on failure, see xerr() */
	xerr_key = keyname;
	xerr_op = opname;
	xerr_prev = XSetErrorHandler(xerr);

	XGrabKey(dpy, code, AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
	XSync(dpy, False);

	XSetErrorHandler(xerr_prev);
	return code;
}

static void handle_xevent(Display *dpy, XEvent *xev)
{
	switch(xev->type) {
	case KeyPress:
		if(xev->xkey.keycode == recenter_keycode) {
			logmsg("recenter...\n");
			ltr_recenter();
		} else if(xev->xkey.keycode == pause_keycode) {
			static int paused;
			if(paused) {
				logmsg("resume...\n");
				ltr_wakeup();
				paused = 0;
			} else {
				logmsg("pause...\n");
				ltr_suspend();
				paused = 1;
			}
		}
		break;

	default:
		break;
	}
}

void shutdown_keyb(void)
{
	if(thread) {
		logmsg("terminating keybindings handler thread\n");

		if(pfd[0] != -1) {
			char c = 'q';
			write(pfd[1], &c, 1);
		} else {
			if(pthread_cancel(thread) == 0) {
				pthread_join(thread, 0);
			}
		}
	} else {
		logmsg("no keybindings thread to terminate\n");
	}
}

static int xerr(Display *dpy, XErrorEvent *err)
{
	logmsg("Failed to bind %s to the %s function. That function will be unavailable!\n", xerr_key, xerr_op);
	return 0;
}
