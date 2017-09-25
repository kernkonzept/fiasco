INTERFACE:

class Jdb_table
{
public:
  enum
  {
    Nothing = 0,
    Handled,
    Redraw,
    Edit,
    Back,
    Exit,
  };

  virtual unsigned col_width(unsigned col) const = 0;
  virtual unsigned long cols() const = 0;
  virtual unsigned long rows() const = 0;
  virtual char col_sep(unsigned col) const;
  virtual void draw_entry(unsigned long row, unsigned long col) = 0;
  virtual unsigned key_pressed(int key, unsigned long &row, unsigned long &col);
  virtual void print_statline(unsigned long row, unsigned long col) = 0;
  virtual bool has_row_labels() const;
  virtual bool has_col_labels() const;
  virtual unsigned width() const;
  virtual unsigned height() const;

  virtual bool edit_entry(unsigned long row, unsigned long col, unsigned cx, unsigned cy);
};


IMPLEMENTATION:

#include <cstdio>
#include "jdb.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "simpleio.h"

IMPLEMENT
bool
Jdb_table::edit_entry(unsigned long, unsigned long, unsigned, unsigned)
{ return false; }

IMPLEMENT
bool
Jdb_table::has_row_labels() const
{ return true; }

IMPLEMENT
bool
Jdb_table::has_col_labels() const
{ return false; }


IMPLEMENT
unsigned
Jdb_table::key_pressed(int, unsigned long &, unsigned long &)
{ return Nothing; }

IMPLEMENT
char
Jdb_table::col_sep(unsigned col) const
{ return col ? ' ' : ':'; }

IMPLEMENT
unsigned
Jdb_table::width() const
{ return Jdb_screen::width(); }

IMPLEMENT
unsigned
Jdb_table::height() const
{ return Jdb_screen::height() - 1; }

PRIVATE
unsigned long
Jdb_table::vis_cols(unsigned long first_col, unsigned long *w)
{
  unsigned c = 0;
  *w = 0;
  unsigned const max_w = width();
  for (; c < cols(); ++c)
    {
      unsigned cw;
      if (c == 0 && has_row_labels())
        cw = col_width(0);
      else
        cw = col_width(first_col + c);

      if (*w + cw > max_w)
        return c;

      *w += cw;
      if (*w == max_w)
        return c + 1;
      else
        *w += 1;
    }
  return c;
}

PRIVATE
unsigned
Jdb_table::col_ofs(unsigned long first_col, unsigned long col)
{
  unsigned long c = 0;
  unsigned long w = 0;
  for (; c < col; ++c)
    {
      if (c == 0 && has_row_labels())
        w += col_width(0) + 1;
      else
        w += col_width(first_col + col) + 1;
    }

  return w;
}

PUBLIC
bool
Jdb_table::show(unsigned long crow, unsigned long ccol)
{
  unsigned long tmp;
  unsigned long absr = 0, absc = 0;
  unsigned long old_absc = 1, old_absr = 1;
  unsigned long dcols, drows;        // drawn columns and rows
  drows = height();
  dcols = vis_cols(absc, &tmp);
  if (drows > rows())
    drows = rows();

  unsigned long max_absr = 0, max_absc = 0;
  if (rows() > drows)
    max_absr = rows() - drows;
  if (cols() > dcols)
    max_absc = cols() - dcols;

  unsigned long min_row, min_col;
  min_row = has_col_labels() ? 1 : 0;
  min_col = has_row_labels() ? 1 : 0;

  absr = crow;
  absc = ccol;

  if (absr > max_absr)
    absr = max_absr;
  if (absc > max_absc)
    absc = max_absc;

  bool redraw = true;

  while (1)
    {
screen:
      if (crow >= rows())
        crow = rows() - 1;
      if (ccol >= cols())
        ccol = cols() - 1;
      if (crow < min_row)
        crow = min_row;
      if (ccol < min_col)
        ccol = min_col;

      if (redraw || old_absc != absc || old_absr != absr)
        {
          dcols = vis_cols(absc, &tmp);
          old_absr = absr;
          old_absc = absc;
          Jdb::cursor(); // go home
          draw_table(absr, absc, drows, dcols);
          redraw = false;
        }

      print_statline(crow, ccol);

      Jdb::cursor(crow - absr + 1,
                  col_ofs(absc, ccol) + 1);

      int c = Jdb_core::getchar();

      unsigned long nrow = crow;
      unsigned long ncol = ccol;
      unsigned kp = key_pressed(c, nrow, ncol);
      if (ncol > ccol)
        {
          ccol = ncol;
          if (ccol - absc >= dcols)
            absc = ccol - dcols + 1;
        }
      else if (ncol < ccol)
        {
          ccol = ncol;
          if (ccol < absc)
            absc = ccol;
        }

      if (nrow > crow)
        {
          crow = nrow;
          if (crow - absr >= drows)
            absr = crow - drows + 1;
        }
      else if (nrow < crow)
        {
          crow = nrow;
          if (crow < absr)
            absr = crow;
        }

      switch (kp)
        {
        case Redraw:
          redraw = true;
          goto screen;
        case Exit:
          return false;
        case Back:
          return true;
        case Handled:
          goto screen;
        case Edit:
            {
              unsigned col_o = col_ofs(absc, ccol) + 1;
              if (edit_entry(crow, ccol, col_o, crow - absr + 1))
                {
                  Jdb::cursor(crow - absr + 1, col_o);
                  draw_entry(crow,ccol);
                  Jdb::cursor(crow - absr + 1, col_o);
                }
            }
          goto screen;

        default:
          break;
        }

      switch (c)
        {
        case KEY_CURSOR_HOME:
        case 'H':
          crow = absr = min_row;
          ccol = absc = min_col;
          break;
        case KEY_CURSOR_END:
        case 'L':
          absr = max_absr;
          absc = max_absc;
          crow = rows() - 1;
          ccol = cols() - 1;
          break;
        case KEY_CURSOR_LEFT:
        case 'h':
          if (ccol > min_col)
            {
              --ccol;
              if (ccol < absc)
                absc = ccol;
            }
          else if (crow > min_row)
            {
              ccol = cols() - 1;
              if (ccol - absc > dcols)
                absc = ccol - dcols;
              --crow;
              if (crow < absr)
                absr = crow;
            }
          break;
        case KEY_CURSOR_RIGHT:
        case 'l':
          if (ccol + 1 < cols())
            {
              ++ccol;
              if (ccol - absc > dcols)
                absc = ccol - dcols;
            }
          else if (crow + 1 < rows())
            {
              absc = ccol = min_col;
              ++crow;
              if (crow - absr >= drows)
                absr = crow - drows + 1;
            }
          break;
        case KEY_CURSOR_UP:
        case 'k':
          if (crow > min_row)
            {
              --crow;
              if (crow < absr)
                absr = crow;
            }
          break;
        case KEY_CURSOR_DOWN:
        case 'j':
          if (crow + 1 < rows())
            {
              ++crow;
              if (crow - absr >= drows)
                absr = crow - drows + 1;
            }
          break;
        case KEY_PAGE_UP:
        case 'K':
          if (crow >= drows + min_row)
            {
              crow -= drows;
              if (absr >= drows)
                absr -= drows;
              else
                absr = 0;
            }
          else
            {
              crow = min_row;
              absr = 0;
            }
          break;
        case KEY_PAGE_DOWN:
        case 'J':
          if (crow + 1 + drows < rows())
            {
              crow += drows;
              if (absr + drows <= max_absr)
                absr += drows;
              else
                absr = max_absr;
            }
          else
            {
              crow = rows() - 1;
              absr = max_absr;
            }
          break;
        case KEY_ESC:
          Jdb::abort_command();
          return false;
        default:
          if (Jdb::is_toplevel_cmd(c))
            return false;
          break;
        }

      if (absc > max_absc)
        absc = max_absc;
      if (absr > max_absr)
        absr = max_absr;
    }
}

PUBLIC
void
Jdb_table::draw_table(unsigned long row, unsigned long col,
                      unsigned lines, unsigned columns)
{
  for (unsigned long y = 0; y < lines; ++y)
    {
      unsigned long w = 0;
      unsigned long r;
      if (has_col_labels() && y == 0)
        r = 0;
      else
        r = row + y;

      Kconsole::console()->getchar_chance();
      for (unsigned long x = 0; x < columns; ++x)
        {
          unsigned long c;
          if (has_row_labels() && x == 0)
            c = 0;
          else
            c = col + x;

          draw_entry(r, c);
          w += col_width(c);
          if (x + 1 < columns)
            {
              putchar(col_sep(c));
              ++w;
            }
        }

      if (w < width())
        {
          if (width() == Jdb_screen::width())
            putstr("\033[K");
          else
            printf("\033[%luX", width() - w); // some consoles don't support this
        }
      putchar('\n');
    }
}
