INTERFACE:

#include "types.h"

class Jdb_screen
{
public:
  static const unsigned Mword_size_bmode = sizeof(Mword) * 2;
  static const unsigned Mword_size_cmode = sizeof(Mword);
  static const unsigned Col_head_size = sizeof(Mword) * 2;

  static const char *Mword_adapter;
  static const char *Mword_not_mapped;
  static const char *Mword_blank;

  static const char *const Reg_names[];
  static const char Reg_prefix;
  static const char *Line;

  static const char *Root_page_table;

private:
  static unsigned int _height;
  static unsigned int _width;
  static bool         _direct_enabled;
};

IMPLEMENTATION:

unsigned int Jdb_screen::_height         = 25; // default for native
unsigned int Jdb_screen::_width          = 80; // default
bool         Jdb_screen::_direct_enabled = true;

const char *Jdb_screen::Mword_adapter    = "~~~~~~~~~~~~~~~~";
const char *Jdb_screen::Mword_not_mapped = "----------------";
const char *Jdb_screen::Mword_blank      = "                ";
const char *Jdb_screen::Line             = "------------------------------------"
                                           "-----------------------------------";

PUBLIC static
void
Jdb_screen::set_height(unsigned int h)
{ _height = h; }

PUBLIC static
void
Jdb_screen::set_width(unsigned int w)
{ _width = w; }

PUBLIC static inline
unsigned int
Jdb_screen::width()
{ return _width; }

PUBLIC static inline
unsigned int
Jdb_screen::height()
{ return _height; }

PUBLIC static inline
unsigned long
Jdb_screen::cols(unsigned headsize, unsigned entrysize)
{ return (width() - headsize) / (entrysize + 1) + 1; }

PUBLIC static inline
unsigned long
Jdb_screen::cols()
{ return cols(Col_head_size, Mword_size_bmode); }

PUBLIC static inline
unsigned long
Jdb_screen::rows()
{ return ((unsigned long)-1) / ((cols() - 1) * 4) + 1; }

PUBLIC static inline
void
Jdb_screen::enable_direct(bool enable)
{ _direct_enabled = enable; }

PUBLIC static inline
bool
Jdb_screen::direct_enabled()
{ return _direct_enabled; }
