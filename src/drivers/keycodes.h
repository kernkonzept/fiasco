/* -*- c++ -*- */

#ifndef KEYCODES_H__
#define KEYCODES_H__

enum
{
  // Keycodes which correspond to their respective ASCII values.
  KEY_BACKSPACE         = 0x08,
  KEY_BACKSPACE_2       = 0x7F,
  KEY_TAB               = 0x09,
  KEY_ESC               = 0x1b,
  KEY_RETURN            = 0x0d,
  KEY_RETURN_2          = 0x0a,

  // Internal keycodes, do not correspond to any official encoding.
  KEY_CURSOR_UP         = 0x1b8,
  KEY_CURSOR_DOWN       = 0x1b2,
  KEY_CURSOR_LEFT       = 0x1b4,
  KEY_CURSOR_RIGHT      = 0x1b6,
  KEY_CURSOR_HOME       = 0x1b7,
  KEY_CURSOR_END        = 0x1b1,
  KEY_PAGE_UP           = 0x1b9,
  KEY_PAGE_DOWN         = 0x1b3,
  KEY_INSERT            = 0x1b0,
  KEY_DELETE            = 0x1ae,
  KEY_F1                = 0x1c0,
  KEY_SINGLE_ESC        = 0x101, ///< Explicit "single-ESC" keycode, indicating that
                                 ///  the ESC is not part of an ESC sequence.
};

#endif // KEYCODES_H__

