INTERFACE:

/**
 * Implementation for PC keyboards.
 */
EXTENSION class Keyb
{
private:

  enum {
    /*
     * Keyboard I/O ports.
     */
    K_RDWR	    = 0x60, /* keyboard data & cmds (read/write) */
    K_STATUS        = 0x64, /* keybd status (read-only) */
    K_CMD	    = 0x64, /* keybd ctlr command (write-only) */

    /*
     * Bit definitions for K_STATUS port.
     */
    K_OBUF_FUL      = 0x01, /* output (from keybd) buffer full */
    K_IBUF_FUL      = 0x02, /* input (to keybd) buffer full */
    K_SYSFLAG	    = 0x04, /* "System Flag" */
    K_CMD_DATA	    = 0x08, /* 1 = input buf has cmd, 0 = data */
    K_KBD_INHIBIT   = 0x10, /* 0 if keyboard inhibited */
    K_AUX_OBUF_FUL  = 0x20, /* 1 = obuf holds aux device data */
    K_TIMEOUT	    = 0x40, /* timout error flag */
    K_PARITY_ERROR  = 0x80, /* parity error flag */

    /*
     * Keyboard controller commands (sent to K_CMD port).
     */
    KC_CMD_READ	    = 0x20, /* read controller command byte */
    KC_CMD_WRITE    = 0x60, /* write controller command byte */
    KC_CMD_DIS_AUX  = 0xa7, /* disable auxiliary device */
    KC_CMD_ENB_AUX  = 0xa8, /* enable auxiliary device */
    KC_CMD_TEST_AUX = 0xa9, /* test auxiliary device interface */
    KC_CMD_SELFTEST = 0xaa, /* keyboard controller self-test */
    KC_CMD_TEST	    = 0xab, /* test keyboard interface */
    KC_CMD_DUMP	    = 0xac, /* diagnostic dump */
    KC_CMD_DISABLE  = 0xad, /* disable keyboard */
    KC_CMD_ENABLE   = 0xae, /* enable keyboard */
    KC_CMD_RDKBD    = 0xc4, /* read keyboard ID */
    KC_CMD_WIN	    = 0xd0, /* read  output port */
    KC_CMD_WOUT	    = 0xd1, /* write output port */
    KC_CMD_ECHO	    = 0xee, /* used for diagnostic testing */
    KC_CMD_PULSE    = 0xff, /* pulse bits 3-0 based on low nybble */

    /*
     * Keyboard commands (send to K_RDWR).
     */
    K_CMD_LEDS	    = 0xed, /* set status LEDs (caps lock, etc.) */

    /*
     * Bit definitions for controller command byte (sent following
     * K_CMD_WRITE command).
     */
    K_CB_ENBLIRQ    = 0x01, /* enable data-ready intrpt */
    K_CB_SETSYSF    = 0x04, /* Set System Flag */
    K_CB_INHBOVR    = 0x08, /* Inhibit Override */
    K_CB_DISBLE	    = 0x10, /* disable keyboard */

    /*
     * Bit definitions for "Indicator Status Byte" (sent after a
     * K_CMD_LEDS command).  If the bit is on, the LED is on.  Undefined
     * bit positions must be 0.
     */
    K_LED_SCRLLK    = 0x1, /* scroll lock */
    K_LED_NUMLK	    = 0x2, /* num lock */
    K_LED_CAPSLK    = 0x4, /* caps lock */

    /*
     * Bit definitions for "Miscellaneous port B" (K_PORTB).
     */
    /* read/write */
    K_ENABLETMR2    = 0x01, /* enable output from timer 2 */
    K_SPKRDATA	    = 0x02, /* direct input to speaker */
    K_ENABLEPRTB    = 0x04, /* "enable" port B */
    K_EIOPRTB	    = 0x08, /* enable NMI on parity error */
    /* read-only */
    K_REFRESHB	    = 0x10, /* refresh flag from INLTCONT PAL */
    K_OUT2B	    = 0x20, /* timer 2 output */
    K_ICKB	    = 0x40, /* I/O channel check (parity error) */

    /*
     * Bit definitions for the keyboard controller's output port.
     */
    KO_SYSRESET	    = 0x01, /* processor reset */
    KO_GATE20	    = 0x02, /* A20 address line enable */
    KO_AUX_DATA_OUT = 0x04, /* output data to auxiliary device */
    KO_AUX_CLOCK    = 0x08, /* auxiliary device clock */
    KO_OBUF_FUL	    = 0x10, /* keyboard output buffer full */
    KO_AUX_OBUF_FUL = 0x20, /* aux device output buffer full */
    KO_CLOCK	    = 0x40, /* keyboard clock */
    KO_DATA_OUT	    = 0x80, /* output data to keyboard */
  };
};

IMPLEMENTATION[pc]:

#include "processor.h"
#include "io.h"

enum {
  SHIFT = 0xff,
};

static unsigned char keymap[][2] = {
  {0       },		/* 0 */
  {27,	27 },		/* 1 - ESC */
  {'1',	'!'},		/* 2 */
  {'2',	'@'},
  {'3',	'#'},
  {'4',	'$'},
  {'5',	'%'},
  {'6',	'^'},
  {'7',	'&'},
  {'8',	'*'},
  {'9',	'('},
  {'0',	')'},
  {'-',	'_'},
  {'=',	'+'},
  {8,	8  },		/* 14 - Backspace */
  {'\t','\t'},		/* 15 */
  {'q',	'Q'},
  {'w',	'W'},
  {'e',	'E'},
  {'r',	'R'},
  {'t',	'T'},
  {'y',	'Y'},
  {'u',	'U'},
  {'i',	'I'},
  {'o',	'O'},
  {'p',	'P'},
  {'[',	'{'},
  {']', '}'},		/* 27 */
  {'\r','\r'},		/* 28 - Enter */
  {0,	0  },		/* 29 - Ctrl */
  {'a',	'A'},		/* 30 */
  {'s',	'S'},
  {'d',	'D'},
  {'f',	'F'},
  {'g',	'G'},
  {'h',	'H'},
  {'j',	'J'},
  {'k',	'K'},
  {'l',	'L'},
  {';',	':'},
  {'\'','"'},		/* 40 */
  {'`',	'~'},		/* 41 */
  {SHIFT, SHIFT},	/* 42 - Left Shift */
  {'\\','|'},		/* 43 */
  {'z',	'Z'},		/* 44 */
  {'x',	'X'},
  {'c',	'C'},
  {'v',	'V'},
  {'b',	'B'},
  {'n',	'N'},
  {'m',	'M'},
  {',',	'<'},
  {'.',	'>'},
  {'/', '?'},		/* 53 */
  {SHIFT, SHIFT},	/* 54 - Right Shift */
  {0,	 0},		/* 55 - Print Screen */
  {0,	 0},		/* 56 - Alt */
  {' ',' '},		/* 57 - Space bar */
  {0,	 0},		/* 58 - Caps Lock */
  {0,	 0},		/* 59 - F1 */
  {0,	 0},		/* 60 - F2 */
  {0,	 0},		/* 61 - F3 */
  {0,	 0},		/* 62 - F4 */
  {0,	 0},		/* 63 - F5 */
  {0,	 0},		/* 64 - F6 */
  {0,	 0},		/* 65 - F7 */
  {0,	 0},		/* 66 - F8 */
  {0,	 0},		/* 67 - F9 */
  {0,	 0},		/* 68 - F10 */
  {0,	 0},		/* 69 - Num Lock */
  {0,	 0},		/* 70 - Scroll Lock */
  {0xb7,0xb7},		/* 71 - Numeric keypad 7 */
  {0xb8,0xb8},		/* 72 - Numeric keypad 8 */
  {0xb9,0xb9},		/* 73 - Numeric keypad 9 */
  {'-',	'-'},		/* 74 - Numeric keypad '-' */
  {0xb4,0xb4},		/* 75 - Numeric keypad 4 */
  {0xb5,0xb5},		/* 76 - Numeric keypad 5 */
  {0xb6,0xb6},		/* 77 - Numeric keypad 6 */
  {'+',	'+'},		/* 78 - Numeric keypad '+' */
  {0xb1,0xb1},		/* 79 - Numeric keypad 1 */
  {0xb2,0xb2},		/* 80 - Numeric keypad 2 */
  {0xb3,0xb3},		/* 81 - Numeric keypad 3 */
  {0xb0,0xb0},		/* 82 - Numeric keypad 0 */
  {0xae,0xae},		/* 83 - Numeric keypad '.' */
};

IMPLEMENT
void
Keyb::set_keymap(Keyb::Keymap km)
{
  // This is a one-time switch over only.
  if (km == Keymap_de)
    {
      // Simple patch to german layout
      keymap[ 3][1] = '"';
      keymap[ 7][1] = '&';
      keymap[ 8][1] = '/';
      keymap[ 9][1] = '(';
      keymap[10][1] = ')';
      keymap[11][1] = '=';
      keymap[12][1] = '?';
      keymap[13][1] = '`';
      keymap[21][0] = 'z';
      keymap[21][1] = 'Z';
      keymap[27][0] = '+';
      keymap[27][1] = '*';
      keymap[41][0] = '^';
      keymap[43][0] = '#';
      keymap[43][1] = '\'';
      keymap[44][0] = 'y';
      keymap[44][1] = 'Y';
      keymap[51][1] = ';';
      keymap[52][1] = ':';
      keymap[53][0] = '-';
      keymap[53][1] = '_';
    }
}

IMPLEMENT
int Keyb::getchar(bool wait)
{
  static unsigned shift_state;
  unsigned status, scan_code, ch;
  Proc::Status old_s = Proc::cli_save();

  for (;;)
    {
      /* Wait until a scan code is ready and read it. */
      status = Io::in8(0x64);
      if ((status & K_OBUF_FUL) == 0)
	{
	  if (wait)
	    continue;
          Proc::sti_restore(old_s);
	  return -1;
	}
      scan_code = Io::in8(0x60);

      /* Drop mouse events */
      if ((status & K_AUX_OBUF_FUL) != 0)
	{
	  if (wait)
	    continue;
	  Proc::sti_restore(old_s);
          return -1;
	}

      if ((scan_code & 0x7f) >= sizeof(keymap)/sizeof(keymap[0]))
	continue;

      /* Handle key releases - only release of SHIFT is important. */
      if (scan_code & 0x80)
	{
	  scan_code &= 0x7f;
	  if (keymap[scan_code][0] == SHIFT)
	    shift_state = 0;
	  continue;
	}

      /* Translate the character through the keymap. */
      ch = keymap[scan_code][shift_state];
      if (ch == (unsigned)SHIFT)
	{
	  shift_state = 1;
	  continue;
	}
      if (ch == 0)
	continue;

      Proc::sti_restore(old_s);
      return ch;
    }
}
