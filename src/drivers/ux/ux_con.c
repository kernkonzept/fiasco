#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>


#include "SDL.h"

#if SDL_MAJOR_VERSION == 2
#define USE_SDL2
#endif

#define PROGNAME "ux_con"

#define DFL_MOUSE_VISIBLE 1
#define DFL_REFRESH_INTERVAL 100

#define WM_WINDOW_TITLE "Fiasco-UX graphical console"
#define WM_ICON_TITLE   "F-UX con"

enum { Do_logging = 0 };
#define LOGGING_FILE "/tmp/ux_con_key.log"

#ifdef USE_SDL2
SDL_Window *window;
#endif

/* this is from libinput.h  --  size == 16 byte */
struct l4input
{
  long long time;		/* used as a marker here */
  unsigned short type;
  unsigned short code;
  unsigned int value;
  unsigned long stream_id;
};

enum {
  SUPERPAGESIZE = 1 << 22,
};

/* needs to be the same as in libinput-ux! */
enum {
  INPUTMEM_SIZE = 1 << 12,
  NR_INPUT_OBJS = INPUTMEM_SIZE / sizeof(struct l4input),
};

enum {
  EV_SYN = 0,
  EV_KEY = 0x01,
  EV_REL = 0x02,

  SYN_REPORT = 0,

  BTN_LEFT    = 0x110,
  BTN_RIGHT   = 0x111,
  BTN_MIDDLE  = 0x112,

  REL_X = 0,
  REL_Y = 1,
};

int physmem_fd;
unsigned long physmem_fb_start;
int width, height, depth;
size_t fb_size;
int refresh_rate;
int mouse_visible;
struct l4input *input_mem;
int input_queue_pos;
int signal_mode, sig_redraw_pending;

struct key_mapping {
#ifdef USE_SDL2
  SDL_Keycode sdlkey;
#else
  unsigned long sdlkey;
#endif
  unsigned long l4ev;
};

#ifdef USE_SDL2
enum
{
  SDLK_KP0 = SDLK_KP_0,
  SDLK_KP1 = SDLK_KP_1,
  SDLK_KP2 = SDLK_KP_2,
  SDLK_KP3 = SDLK_KP_3,
  SDLK_KP4 = SDLK_KP_4,
  SDLK_KP5 = SDLK_KP_5,
  SDLK_KP6 = SDLK_KP_6,
  SDLK_KP7 = SDLK_KP_7,
  SDLK_KP8 = SDLK_KP_8,
  SDLK_KP9 = SDLK_KP_9,
};
#endif

/*
 * This table translates from a german SDL map to a normal keyboard.
 * Other keymaps on the host keyboard should give weird behavior
 * (patches welcome).
 */
static struct key_mapping key_map[] = {
  { SDLK_ESCAPE,       1 },
  { SDLK_1,            2 },
  { SDLK_2,            3 },
  { SDLK_3,            4 },
  { SDLK_4,            5 },
  { SDLK_5,            6 },
  { SDLK_6,            7 },
  { SDLK_7,            8 },
  { SDLK_8,            9 },
  { SDLK_9,            10 },
  { SDLK_0,            11 },
  { SDLK_MINUS,        53 },
  { SDLK_EQUALS,       13 },
  { SDLK_BACKSPACE,    14 },
  { SDLK_TAB,          15 },
  { SDLK_q,            16 },
  { SDLK_w,            17 },
  { SDLK_e,            18 },
  { SDLK_r,            19 },
  { SDLK_t,            20 },
  { SDLK_z,            21 },
  { SDLK_u,            22 },
  { SDLK_i,            23 },
  { SDLK_o,            24 },
  { SDLK_p,            25 },
  { SDLK_LEFTBRACKET,  26 },
  { SDLK_RIGHTBRACKET, 27 },
  { SDLK_RETURN,       28 },
  { SDLK_LCTRL,        29 },
  { SDLK_a,            30 },
  { SDLK_s,            31 },
  { SDLK_d,            32 },
  { SDLK_f,            33 },
  { SDLK_g,            34 },
  { SDLK_h,            35 },
  { SDLK_j,            36 },
  { SDLK_k,            37 },
  { SDLK_l,            38 },
  { SDLK_SEMICOLON,    39 },
  { SDLK_QUOTE,        13 },
  { SDLK_BACKQUOTE,    41 },
  { SDLK_LSHIFT,       42 },
  { SDLK_BACKSLASH,    43 },
  { SDLK_y,            44 },
  { SDLK_x,            45 },
  { SDLK_c,            46 },
  { SDLK_v,            47 },
  { SDLK_b,            48 },
  { SDLK_n,            49 },
  { SDLK_m,            50 },
  { SDLK_COMMA,        51 },
  { SDLK_PERIOD,       52 },
  { SDLK_SLASH,        53 },
  { SDLK_RSHIFT,       54 },
  { SDLK_KP_MULTIPLY,  55 },
  { SDLK_LALT,         56 },
  { SDLK_SPACE,        57 },
  { SDLK_CAPSLOCK,     58 },
  { SDLK_F1,           59 },
  { SDLK_F2,           60 },
  { SDLK_F3,           61 },
  { SDLK_F4,           62 },
  { SDLK_F5,           63 },
  { SDLK_F6,           64 },
  { SDLK_F7,           65 },
  { SDLK_F8,           66 },
  { SDLK_F9,           67 },
  { SDLK_F10,          68 },
#ifndef USE_SDL2
  { SDLK_NUMLOCK,      69 },
  { SDLK_SCROLLOCK,    70 },
#else
  { SDLK_SCROLLLOCK,   70 },
#endif
  { SDLK_KP7,          71 }, /* Keypad 7 */
  { SDLK_KP8,          72 }, /* Keypad 8 */
  { SDLK_KP9,          73 }, /* Keypad 9 */
  { SDLK_KP_MINUS,     74 }, /* Keypad minus */
  { SDLK_KP4,          75 }, /* Keypad 4 */
  { SDLK_KP5,          76 }, /* Keypad 5 */
  { SDLK_KP6,          77 }, /* Keypad 6 */
  { SDLK_KP_PLUS,      78 }, /* Keypad plus */
  { SDLK_KP1,          79 }, /* Keypad 1 */
  { SDLK_KP2,          80 }, /* Keypad 2 */
  { SDLK_KP3,          81 }, /* Keypad 3 */
  { SDLK_KP0,          82 }, /* Keypad 0 */
  { SDLK_KP_PERIOD,    83 }, /* Keypad dot */

  { SDLK_F11,          87 },
  { SDLK_F12,          88 },
  { SDLK_F13,          85 },
  { SDLK_F14,          89 },
  { SDLK_F15,          90 },

  { SDLK_KP_ENTER,     96 }, /* Keypad enter */
  { SDLK_RCTRL,        97 },
  { SDLK_KP_DIVIDE,    98 }, /* Keypad slash */
#ifdef USE_SDL2
  { SDLK_PRINTSCREEN,  99 },
#else
  { SDLK_PRINT,        99 },
#endif
  { SDLK_RALT,         100 },
  { SDLK_MODE,         100 }, /* Alt Gr */

  { SDLK_HOME,         102 },
  { SDLK_UP,           103 },
  { SDLK_PAGEUP,       104 },
  { SDLK_LEFT,         105 },
  { SDLK_RIGHT,        106 },
  { SDLK_END,          107 },
  { SDLK_DOWN,         108 },
  { SDLK_PAGEDOWN,     109 },
  { SDLK_INSERT,       110 },
  { SDLK_DELETE,       111 },

  { SDLK_KP_EQUALS,    117 }, /* Keypad equals */

  { SDLK_PAUSE,        119 },

#ifdef USE_SDL2
  { SDLK_LGUI,         125 },
  { SDLK_RGUI,         126 },
#else
  { SDLK_LSUPER,       125 }, /* Left "Penguin" key */
  { SDLK_RSUPER,       126 }, /* Right "Penguin" key */
#endif
  { SDLK_MENU,         127 },

  { 223,               12 }, /* sz */
  { 252,               26 }, /* ue */
  { 43,                27 }, /* + -> ] */
  { 246,               39 }, /* oe  -> ; */
  { 228,               40 }, /* ae  -> " */
  { 94,                41 }, /* ^ ° -> ` */
  { 35,                43 }, /* # ' -> backslash */
  { 60,                86 }, /* < > | */

  { 0, 0},
};

FILE *log_fd;
#define DO_LOG(x...) \
  do { if (Do_logging) { fprintf(log_fd, x); fflush(log_fd); } } while (0)

static void start_logging(void)
{
  if (Do_logging)
    log_fd = fopen(LOGGING_FILE, "a");
}

static void end_logging(void)
{
  if (Do_logging)
    fclose(log_fd);
}

#ifdef USE_SDL2
static unsigned long map_keycode(SDL_Keycode sk)
#else
static unsigned long map_keycode(SDLKey sk)
#endif
{
  unsigned int i;

  for (i = 0; key_map[i].sdlkey; i++)
    if (key_map[i].sdlkey == sk) {
      DO_LOG("%s: keytrans: #%d (%s) -> %ld\n",
	     PROGNAME, sk, SDL_GetKeyName(sk), key_map[i].l4ev);
      return key_map[i].l4ev;
    }

  DO_LOG("%s: Unknown key pressed/released: #%d (%s)\n",
         PROGNAME, sk, SDL_GetKeyName(sk));
  return 0;
}

void usage(char *prog)
{
  printf("%s options\n\n"
         "  -f fd        File descriptor to mmap\n"
         "  -s value     Start of FB in physmem (decimal value)\n"
         "  -x val       Width of screen in pixels\n"
         "  -y val       Height of screen in pixels\n"
         "  -d val       Color depth in bits\n"
         "  -r val       Refresh rate in ms\n"
	 "  -m           Flip mouse visibility\n"
	 "  -F           Switch to fullscreen mode\n"
         ,prog);
}

static void set_window_title(int count)
{
  char title[100];

  snprintf(title, sizeof(title), "%s - %s | %d",
           WM_WINDOW_TITLE,
	   signal_mode ? "Signalled mode" : "Polling mode", count);

#ifdef USE_SDL2
  SDL_SetWindowTitle(window, title);
#else
  SDL_WM_SetCaption(title, WM_ICON_TITLE);
#endif
}

Uint32 timer_call_back(Uint32 interval, void *param)
{
  static SDL_Event se = { .type = SDL_USEREVENT };

  (void)param;

  SDL_PushEvent(&se);

  return interval;
}

static inline int enqueue_event(struct l4input e)
{
  struct l4input *p = input_mem + input_queue_pos;

  if (p->time) {
    printf("Ringbuffer overflow, don't type/move too fast!\n");
    DO_LOG("Ringbuffer overflow, don't type/move too fast!\n");
    return 1;
  }

  e.time = SDL_GetTicks() * 1000ULL + 1;
  *p = e;

  input_queue_pos++;
  if (input_queue_pos == NR_INPUT_OBJS)
    input_queue_pos = 0;
  return 0;
}

static inline void generate_irq(void)
{
  struct l4input e;
  e.type  = EV_SYN;
  e.code  = SYN_REPORT;
  e.value = e.stream_id = 0;
  enqueue_event(e);


  if (write(0, "I", 1) != 1) {
    DO_LOG("%s: Communication problems with Fiasco-UX, dying...!\n",
	   PROGNAME);
    exit(0);
  }
}

static void propagate_event(struct l4input e)
{
  if (!enqueue_event(e))
    generate_irq();
}

void signal_handler(int sig)
{
  (void)sig;
  signal_mode = sig_redraw_pending = 1;
}

static void loop(SDL_Surface *surface, void *fbmem)
{
  SDL_Event e;
  SDL_TimerID timer_id;
  int count = 0;
  int poll_mode_shown = 1;

  /* Install signal handler for SIGUSR1 */
  signal(SIGUSR1, signal_handler);

  if ((timer_id = SDL_AddTimer(refresh_rate, timer_call_back, NULL)) == (SDL_TimerID)0) {
    fprintf(stderr, "%s: Adding of timer failed!", PROGNAME);
    exit(1);
  }

  while (SDL_WaitEvent(&e)) {
    struct l4input l4e = { .time = 0, .code = 0,
                           .type = 0, .value = 0, .stream_id = 0 };

    if (signal_mode && poll_mode_shown) {
      set_window_title(count);
      poll_mode_shown = 0;
    }

    if (count % 100 == 0)
      set_window_title(count);

    switch (e.type) {
      case SDL_KEYUP:
      case SDL_KEYDOWN:
	l4e.value = e.type == SDL_KEYDOWN;
	l4e.type = EV_KEY;
	l4e.code = map_keycode(e.key.keysym.sym);

	//fprintf(stderr, "%s: sdlkey = %d  l4e.code = %d\n", __func__, e.key.keysym.sym, l4e.code);

	propagate_event(l4e);

	break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
	l4e.value = e.type == SDL_MOUSEBUTTONDOWN;
	l4e.type = EV_KEY;

	switch (e.button.button) {
	  case SDL_BUTTON_LEFT:
	          l4e.code = BTN_LEFT;
	          break;
          case SDL_BUTTON_RIGHT:
		  l4e.code = BTN_RIGHT;
		  break;
	  case SDL_BUTTON_MIDDLE:
		  l4e.code = BTN_MIDDLE;
		  break;
	  default:
		  l4e.code = 0;
	}

	propagate_event(l4e);

	break;

      case SDL_MOUSEMOTION:
	{
	  int ret = 1;

	  l4e.type = EV_REL;
	  if (e.motion.xrel) {
	    l4e.code = REL_X;
	    l4e.value = e.motion.xrel;
	    ret = enqueue_event(l4e);
	  }
	  if (e.motion.yrel) {
	    l4e.code = REL_Y;
	    l4e.value = e.motion.yrel;
	    ret |= enqueue_event(l4e);
	  }
	  if (!ret)
	    generate_irq();
	}

	break;

#ifdef USE_SDL2
      case SDL_WINDOWEVENT:
	break;
#else
      case SDL_ACTIVEEVENT: /* Window (non-)active */
	break;
#endif

      case SDL_QUIT:
	exit(0);

      case SDL_USEREVENT:
	if (!signal_mode || sig_redraw_pending) {
	  /* Redraw screen */
	  if (SDL_LockSurface(surface) < 0)
	    break;
	  memcpy(surface->pixels, fbmem, fb_size);
	  SDL_UnlockSurface(surface);
#ifdef USE_SDL2
          SDL_UpdateWindowSurface(window);
#else
	  SDL_Flip(surface);
#endif
          count++;
	  sig_redraw_pending = 0;
	}
	break;

      default:
	fprintf(stderr, "%s: Unknown event: %d\n", PROGNAME, e.type);
	DO_LOG("Unknown event: %d\n", e.type);
	break;
    }

  }

}

static void launch_me(char *p)
{
  enum { W = 640, H = 480, B = 32 };
  const char *filename = "/tmp/ux_con_test";
  unsigned long offset = 0x1000;

  int r;

  int fd = open(filename, O_RDWR | O_CREAT);
  assert(fd != -1);

  r = unlink(filename);
  assert(r != -1);

  size_t s = (((W * H * ((B + 7) / 8)) & ~(SUPERPAGESIZE - 1))) + SUPERPAGESIZE;
  s += offset;
  s += INPUTMEM_SIZE;

  r = lseek(fd, s, SEEK_SET);
  assert(r != -1);

  r = write(fd, ".", 1);
  assert(r == 1);

  pid_t pid = fork();
  switch (pid)
    {
    case 0:
      {
        char s_fd[3];
        char s_width[5];
        char s_height[5];
        char s_depth[3];
        char s_fbaddr[15];

        snprintf(s_fd,     sizeof(s_fd),     "%u", fd);
        snprintf(s_fbaddr, sizeof(s_fbaddr), "%lu", offset);
        snprintf(s_width,  sizeof(s_width),  "%u", W);
        snprintf(s_height, sizeof(s_height), "%u", H);
        snprintf(s_depth,  sizeof(s_depth),  "%u", B);

        execl(p, p, "-f", s_fd, "-s", s_fbaddr,
              "-x", s_width, "-y", s_height, "-d", s_depth, NULL);
        perror("execl");
        exit(1);
      }
    case -1:
      perror("fork");
      exit(1);
    default:
      break;
    };

  int status;
  waitpid(pid, &status, 0);
  fprintf(stderr, "returned %d\n", status);
  exit(0);
}

int main(int argc, char **argv)
{
  int c;
  void *fbmem;
  SDL_Surface *surface;
  unsigned int sdl_video_flags = 0; /* SDL_DOUBLEBUF; */

  refresh_rate = DFL_REFRESH_INTERVAL;
  mouse_visible = DFL_MOUSE_VISIBLE;

  while ((c = getopt(argc, argv, "f:x:y:s:d:r:mFLW")) != -1) {
    switch (c) {
      case 'f':
	physmem_fd = atoi(optarg);
	break;
      case 's':
	physmem_fb_start = atol(optarg);
	break;
      case 'x':
	width = atoi(optarg);
	break;
      case 'y':
	height = atoi(optarg);
	break;
      case 'd':
	depth = atoi(optarg);
	break;
      case 'r':
	refresh_rate = atoi(optarg);
	break;
      case 'm':
	mouse_visible = !mouse_visible;
	break;
      case 'L':
        launch_me(*argv);
        break;
      case 'W':
        printf("PID: %d\n", getpid());
        sleep(5);
        break;
      case 'F':
#ifdef USE_SDL2
	sdl_video_flags |= SDL_WINDOW_FULLSCREEN;
#else
	sdl_video_flags |= SDL_FULLSCREEN;
#endif
	break;
      default:
	usage(*argv);
	break;
    }
  }

  start_logging();
  atexit(end_logging);

  if (!physmem_fd || !physmem_fb_start || !width || !height || !depth ||
      !refresh_rate) {
    fprintf(stderr, "%s: Invalid arguments!\n", PROGNAME);
    usage(*argv);
    exit(1);
  }

  printf("Frame buffer resolution of UX-con: %dx%d@%d, refresh: %dms\n",
         width, height, depth, refresh_rate);

  int depth_bytes;
  depth_bytes = ((depth + 7) >> 3);
  fb_size = width * height * depth_bytes;

  size_t physmem_size;
  physmem_size = ((fb_size & ~(SUPERPAGESIZE - 1))) + SUPERPAGESIZE;

  if ((fbmem = mmap(NULL, physmem_size + INPUTMEM_SIZE,
	            PROT_READ | PROT_WRITE, MAP_SHARED,
	            physmem_fd, physmem_fb_start)) == MAP_FAILED) {
    fprintf(stderr, "%s: mmap failed: %s\n", PROGNAME, strerror(errno));
    exit(1);
  }

  input_mem = (struct l4input *)((char *)fbmem + physmem_size);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "%s: Can't init SDL: %s\n",
	    PROGNAME, SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);

#ifdef USE_SDL2
  window = SDL_CreateWindow(WM_WINDOW_TITLE,
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            width, height, SDL_WINDOW_SHOWN | sdl_video_flags);
  if (window == NULL)
#else
  surface = SDL_SetVideoMode(width, height, depth, sdl_video_flags);
  if (surface == NULL)
#endif
    {
      fprintf(stderr, "%s: Couldn't init video mode: %s\n",
              PROGNAME, SDL_GetError());
      exit(1);
    }

#ifdef USE_SDL2
    {
      // somehow we have to add the 'depth' value to the game, but in the
      // end I do not see how I should connect the created surface with the
      // window, or otherwise bring the surface to the window, maybe it
      // works with textures and (software)renderers, but at least it's not
      // obvious to me
      // the good thing would be here that we can give fbmem as the pixel
      // data and would avoid the periodic memcpy
      // currently you have to use the same depth as your desktop
      if (0)
        {
          surface = SDL_CreateRGBSurfaceFrom(fbmem, width, height, depth,
              width * ((depth + 7) / 8),
              0, 0, 0, 0);
          if (!surface)
            {
              printf("%s\n", SDL_GetError());
              exit(1);
            }
        }
      else
        surface = SDL_GetWindowSurface(window);
    }
#endif

  set_window_title(0);

  SDL_ShowCursor(mouse_visible);

  loop(surface, fbmem);

  return 0;
}
