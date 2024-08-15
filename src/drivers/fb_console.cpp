// ------------------------------------------------------------------------
INTERFACE [!fb_console]:

class Fb_console
{
public:
  static void init() {}
};

// ------------------------------------------------------------------------
INTERFACE [fb_console]:

#include "types.h"
#include "console.h"

class Fb_console : public Console
{
public:
  void clear();
  void scroll(unsigned n);
  Address video_base() const;
  void video_base(Address base);
  Fb_console(Address base, unsigned width, unsigned height, unsigned scanline,
             bool light_white = false, bool use_color = false);

  ~Fb_console();
  int write(char const *str, size_t len) override;

  int getchar(bool blocking = true) override;

  inline bool is_working();

  inline void printchar(unsigned x, unsigned y,
                        unsigned char c, unsigned char a);


  typedef unsigned long long l4_uint64_t;
  typedef unsigned l4_uint32_t;
  typedef unsigned short l4_uint16_t;
  typedef unsigned char l4_uint8_t;


  typedef struct
  {
    l4_uint64_t flags;          /**< Flags */
    l4_uint64_t cmdline;        /**< Pointer to kernel command line */
    l4_uint64_t mods_addr;      /**< Module list */
    l4_uint32_t mods_count;     /**< Number of modules */
    l4_uint32_t _pad;

    /**
     * VESA video info, valid if one of vbe_ctrl_info
     * or vbe_mode_info is not zero.
     */
    l4_uint64_t vbe_ctrl_info;  /**< VESA video controller info */
    l4_uint64_t vbe_mode_info;  /**< VESA video mode info */
  } l4util_l4mod_info;


  /** VBE controller information. */
  typedef struct
  {
    l4_uint8_t signature[4];
    l4_uint16_t version;
    l4_uint32_t oem_string;
    l4_uint32_t capabilities;
    l4_uint32_t video_mode;
    l4_uint16_t total_memory;
    l4_uint16_t oem_software_rev;
    l4_uint32_t oem_vendor_name;
    l4_uint32_t oem_product_name;
    l4_uint32_t oem_product_rev;
    l4_uint8_t reserved[222];
    l4_uint8_t oem_data[256];
  } __attribute__((packed)) l4util_mb_vbe_ctrl_t;
  static_assert(sizeof(l4util_mb_vbe_ctrl_t) == 512, "Check l4util_mb_vbe_ctrl_t");


  /** VBE mode information. */
  typedef struct
  {
    /** @name all VESA versions
    * @{ */
    /** Mode attributes. */
    l4_uint16_t mode_attributes;
    /** Window A attributes. */
    l4_uint8_t win_a_attributes;
    /** Window B attributes. */
    l4_uint8_t win_b_attributes;
    /** Window granularity. */
    l4_uint16_t win_granularity;
    /** Window size. */
    l4_uint16_t win_size;
    /** Window A start segment. */
    l4_uint16_t win_a_segment;
    /** Window B start segment. */
    l4_uint16_t win_b_segment;
    /** Real mode pointer to window function. */
    l4_uint32_t win_func;
    /** Bytes per scan line. */
    l4_uint16_t bytes_per_scanline;
    /** @} */

    /** @name >= VESA version 1.2
     * @{ */
    /** Horizontal resolution in pixels or characters. */
    l4_uint16_t x_resolution;
    /** Vertical resolution in pixels or characters. */
    l4_uint16_t y_resolution;
    /** Character cell width in pixels. */
    l4_uint8_t x_char_size;
    /** Character cell height in pixels. */
    l4_uint8_t y_char_size;
    /** Number of memory planes. */
    l4_uint8_t number_of_planes;
    /** Bits per pixel. */
    l4_uint8_t bits_per_pixel;
    /** Number of banks. */
    l4_uint8_t number_of_banks;
    /** Memory model type. */
    l4_uint8_t memory_model;
    /** Bank size in KiB. */
    l4_uint8_t bank_size;
    /** Number of images. */
    l4_uint8_t number_of_image_pages;
    /** Reserved for page function. */
    l4_uint8_t reserved0;
    /** @} */

    /** @name direct color
     * @{ */
    /** Size of direct color red mask in bits. */
    l4_uint8_t red_mask_size;
    /** Bit position of LSB of red mask. */
    l4_uint8_t red_field_position;
    /** Size of direct color green mask in bits. */
    l4_uint8_t green_mask_size;
    /** Bit position of LSB of green mask. */
    l4_uint8_t green_field_position;
    /** Size of direct color blue mask in bits. */
    l4_uint8_t blue_mask_size;
    /** Bit position of LSB of blue mask. */
    l4_uint8_t blue_field_position;
    /** Size of direct color reserved mask in bits. */
    l4_uint8_t reserved_mask_size;
    /** Bit position of LSB of reserved mask. */
    l4_uint8_t reserved_field_position;
    /** Direct color mode attributes. */
    l4_uint8_t direct_color_mode_info;
    /** @} */

    /** @name >= VESA version 2.0
     * @{*/
    /** Physical address for flat memory memory frame buffer. */
    l4_uint32_t phys_base;
    /** Reserved -- always set to 0. */
    l4_uint32_t reserved1;
    /** Reserved -- always set to 0. */
    l4_uint16_t reversed2;
    /** @} */

    /** @name >= VESA version 3.0
     * @{*/
    /** Bytes per scan line for linear modes. */
    l4_uint16_t linear_bytes_per_scanline;
    /** Number of images for banked modes. */
    l4_uint8_t banked_number_of_image_pages;
    /** Number of images for linear modes. */
    l4_uint8_t linear_number_of_image_pages;
    /** Size of direct color red mask (linear modes). */
    l4_uint8_t linear_red_mask_size;
    /** Bit position of LSB of red mask (linear modes). */
    l4_uint8_t linear_red_field_position;
    /** Size of direct color green mask (linear modes). */
    l4_uint8_t linear_green_mask_size;
    /** Bit position of LSB of green mask (linear modes). */
    l4_uint8_t linear_green_field_position;
    /** Size of direct color blue mask (linear modes). */
    l4_uint8_t linear_blue_mask_size;
    /** Bit position of LSB of blue mask (linear modes). */
    l4_uint8_t linear_blue_field_position;
    /** Size of direct color reserved mask (linear modes). */
    l4_uint8_t linear_reserved_mask_size;
    /** Bit position of LSB of reserved mask (linear modes). */
    l4_uint8_t linear_reserved_field_position;
    /** Maximum pixel clock (in Hz) for graphics mode. */
    l4_uint32_t max_pixel_clock;

    l4_uint8_t reserved3[190];
    /** @} */
  } __attribute__ ((packed)) l4util_mb_vbe_mode_t;
  static_assert(sizeof(l4util_mb_vbe_mode_t) == 256, "Check l4util_mb_vbe_mode_t");

  enum { L4UTIL_MB_VIDEO_INFO = 0x00000800 };

  static void init();

private:

  /// Type of a on screen character.
  struct Pixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned value() const { return r | (g << 8) | (b << 16); }
  } __attribute__((packed));

  void *_video_base;
  unsigned _x, _y;
  unsigned _attribute;
  enum {
    MAX_ANSI_ESC_ARGS = 5,
  };

  int ansi_esc_args[MAX_ANSI_ESC_ARGS];
  unsigned num_ansi_esc_args;

  struct Screen
  {
    unsigned size() { return width * height * 2; };
    void set(unsigned idx, char ch, char attr)
    {
      s[idx].ch = ch;
      s[idx].attr = attr;
    }
    char get_ch(unsigned idx)   { return s[idx].ch; }
    char get_attr(unsigned idx) { return s[idx].attr; }

    struct Screen_char
    {
      Unsigned8 ch;
      Unsigned8 attr;
    };

    union
    {
      Screen_char *s;
      void      *raw;
    };
    unsigned width, height;
  };

  Screen _screen;

  void (Fb_console::*wr)(char const *, size_t, unsigned & );

  bool const _light_white;
  bool const _use_color;
  unsigned _pixel_width;
  unsigned _pixel_height;
  unsigned _scanline;

  /**
   * Set blinking screen cursor
   */
  void blink_cursor(unsigned x, unsigned y);

  unsigned _scroll_delta;

  enum
  {
    Psf_v1_magic_1 = 0x36,
    Psf_v1_magic_2 = 0x04,
  };
  struct psf_v1_font_header
  {
    unsigned char magic[2];
    unsigned char mode;
    unsigned char height;
  } __attribute__((packed));

  enum
  {
    Psf_v2_magic_1 = 0x72,
    Psf_v2_magic_2 = 0xb5,
    Psf_v2_magic_3 = 0xa4,
    Psf_v2_magic_4 = 0x86,
  };
  struct psf_v2_font_header
  {
    unsigned char magic[4];
    unsigned int version;
    unsigned int headersize;
    unsigned int flags;
    unsigned int numglyph;
    unsigned int bytesperglyph;
    unsigned int height;
    unsigned int width;
  } __attribute__((packed));

  struct psf_v1_font
  {
    struct psf_v1_font_header header;
    char data[];
  };

  struct psf_v2_font
  {
    struct psf_v2_font_header header;
    char data[];
  };

  union psf_font
  {
    psf_v1_font v1;
    psf_v2_font v2;
  };

  unsigned _font_height;
  unsigned _font_width;
  const char *_font_data = nullptr;
};


// ------------------------------------------------------------------------
IMPLEMENTATION [fb_console]:

#include <cstdio>
#include <cstring>
#include <cctype>

#include "io.h"
#include "mem_layout.h"
#include "kip.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "kernel_console.h"

static Static_object<Fb_console> fbcon;

IMPLEMENT static
void Fb_console::init()
{
  // MBI and VBx areas could be mapped cached also
  Address _mbi = Kmem::mmio_remap(Kip::k()->user_ptr, Config::PAGE_SIZE, true);
  assert(_mbi != ~0UL);
  l4util_l4mod_info *mbi = reinterpret_cast<l4util_l4mod_info *>(_mbi);

  if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
    {
      Address _vbe = Kmem::mmio_remap(mbi->vbe_ctrl_info, Config::PAGE_SIZE, true);
      assert(_vbe != ~0UL);
      Address _vbi = Kmem::mmio_remap(mbi->vbe_mode_info, Config::PAGE_SIZE, true);
      assert(_vbi != ~0UL);

      l4util_mb_vbe_ctrl_t *vbe = reinterpret_cast<l4util_mb_vbe_ctrl_t *>(_vbe);
      l4util_mb_vbe_mode_t *vbi = reinterpret_cast<l4util_mb_vbe_mode_t *>(_vbi);


      // vbi->phys_base + vbi->reserved1 form the 64 bit phys address
      // FB could be mapped more cached aware
      Address fbphys = vbi->phys_base + (static_cast<Unsigned64>(vbi->reserved1) << 32);
      Address fbmem = Kmem::mmio_remap(fbphys, vbe->total_memory * (64 << 10));
      assert(fbmem != ~0UL);

      unsigned w = vbi->x_resolution;
      unsigned h = vbi->y_resolution;
      unsigned s = vbi->bytes_per_scanline;
      printf("fbmem virt/phys=%lx/%lx  XxY: %ux%u (%u)\n",
             fbmem, static_cast<unsigned long>(fbphys), w, h, s);

      // TODO: At assert on the pixel-format
      // Extended-TODO: Add support for other pixel formats

      fbcon.construct(fbmem, w, h, s, true, true);
      Kconsole::console()->register_console(fbcon);
    }
}

IMPLEMENT
Fb_console::Fb_console(Address vbase, unsigned pixel_width, unsigned pixel_height,
                       unsigned scanline, bool light_white, bool use_color)
: Console(ENABLED), _video_base(reinterpret_cast<void *>(vbase)),
  _x(0), _y(0), _attribute(light_white ? 0x0f : 0x07),
  wr(&Fb_console::normal_write), _light_white(light_white),
  _use_color(use_color),
  _pixel_width(pixel_width), _pixel_height(pixel_height),
  _scanline(scanline),
  _scroll_delta(1) // tunable
{
  font_init();

  _screen.width = _pixel_width / _font_width;
  _screen.height = _pixel_height / _font_height;
  _screen.raw = Kmem_alloc::allocator()->alloc(Bytes(_screen.size()));
  assert(_screen.raw);

  clear();
}

PRIVATE
void Fb_console::font_init()
{
  extern const char _binary_vgafont_psf_start[];
  const psf_font *font = reinterpret_cast<const psf_font *>(_binary_vgafont_psf_start);

  if (   font->v1.header.magic[0] == Psf_v1_magic_1
      && font->v1.header.magic[1] == Psf_v1_magic_2)
    {
      _font_width = 8;
      _font_height = font->v1.header.height;
      _font_data = font->v1.data;
    }
  else if (   font->v2.header.magic[0] == Psf_v2_magic_1
           && font->v2.header.magic[1] == Psf_v2_magic_2
           && font->v2.header.magic[2] == Psf_v2_magic_3
           && font->v2.header.magic[3] == Psf_v2_magic_4)
    {
      if (font->v2.header.width != 8)
        return; // Unfortunaly we only support 8bit width for now
      if (font->v2.header.flags)
        return; // we do not know about unicode tables
      if (font->v2.header.numglyph < 256)
        return; // we use 256 chars
      _font_width  = font->v2.header.width;
      _font_height = font->v2.header.height;
      _font_data = font->v2.data;
    }
  // else: remain disabled
}

PRIVATE inline
void Fb_console::set(unsigned i, char c, char attr)
{
  _screen.set(i, c, attr);

  if (!_font_data)
    return;

  static Pixel colors[] =
    {
      // normal
      { 0,      0,   0, 0xff }, // 0: black
      { 0,      0, 255, 0xff }, // 1: blue
      { 0,    255,   0, 0xff }, // 2: green
      { 0,    255, 255, 0xff }, // 3: cyan
      { 0xff,   0,   0, 0xff }, // 4: red
      { 255,    0, 255, 0xff }, // 5: magenta
      { 255,  255,   0, 0xff }, // 6: yellow
      { 255,  255, 255, 0xff }, // 7: white
      // bolder
      { 0,      0,   0, 0xff }, // 0: black
      { 0,      0, 255, 0xff }, // 1: blue
      { 0,    255,   0, 0xff }, // 2: green
      { 0,    255, 255, 0xff }, // 3: cyan
      { 0xff,   0,   0, 0xff }, // 4: red
      { 255,    0, 255, 0xff }, // 5: magenta
      { 255,  255,   0, 0xff }, // 6: yellow
      { 255,  255, 255, 0xff }, // 7: white
    };

  unsigned vx, vy;
  unsigned x = i % _screen.width;
  unsigned y = i / _screen.width; // TODO: avoid division

  vx = x * _font_width;
  vy = y * _font_height;

  // TODO: do better
  if (vx + _font_width > _pixel_width || vy + _font_height > _pixel_height)
    return;

  char const *fontchar = &_font_data[(_font_width / 8) * _font_height * c];

  unsigned fg = colors[attr & 0xf].value();
  unsigned bg = colors[(attr >> 4) & 0xf].value();
  unsigned *pos = reinterpret_cast<unsigned *>(_video_base) + _scanline / 4 * vy + vx;

  for (unsigned j = 0; j < _font_height; ++j)
    {
      for (unsigned i = 0; i < 8; ++i)
        {
          unsigned color = (fontchar[j] & (1 << (7 - i))) ? fg : bg;
          pos[i] = color;
        }
      pos += _scanline / 4;
    }
}

IMPLEMENT
void Fb_console::scroll(unsigned lines)
{
  if (lines == 0)
    return;

  if (lines > _screen.height)
    lines = _screen.height;

  unsigned offset = lines * _screen.width;
  for (unsigned i = 0; i < (_screen.height - lines) * _screen.width; ++i)
    set(i, _screen.get_ch(i + offset), _screen.get_attr(i + offset));

  for (unsigned i = (_screen.height - lines) * _screen.width; i < _screen.width * _screen.height; ++i)
    set(i, 0x20, _attribute);
}

IMPLEMENT inline NEEDS[Fb_console::set]
void
Fb_console::printchar(unsigned x, unsigned y,
                      unsigned char c, unsigned char a)
{
  set(x + y * _screen.width, c, a);
}

IMPLEMENT
void Fb_console::blink_cursor(unsigned x, unsigned y)
{
  (void)x;
  (void)y;
}

IMPLEMENT
void Fb_console::clear()
{
  for (unsigned i = 0; i < _screen.width * _screen.height; ++i)
    set(i, 0x20, _attribute);
}

PRIVATE inline
int Fb_console::seq_6(char const *str, size_t len, unsigned &pos)
{
  if (pos + 2 >= len)
    return 0;
  _y = str[pos + 1];
  _x = str[pos + 2];
  if (_y >= _screen.height)
    _y = _screen.height - 1;
  if (_x >= _screen.width)
    _x = _screen.width - 1;
  pos += 2;
  return 1;
}

PRIVATE inline
int Fb_console::seq_1(char const *, size_t, unsigned &)
{
  _x = 0;
  _y = 0;
  return 1;
}

PRIVATE inline NEEDS[Fb_console::set]
int Fb_console::seq_5(char const *, size_t, unsigned &)
{
  for (unsigned i = 0; i < _screen.width - _x; ++i)
    set(_x + (_y * _screen.width) + i, 0x20, _attribute);

  return 1;
}


PRIVATE inline
void Fb_console::ansi_attrib(int a)
{
  char const colors[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

  if (!_use_color && a >= 30 && a <= 47)
    return;

  switch (a)
  {
  case 0:
    if (_light_white)
      _attribute = 0x0f;
    else
      _attribute = 0x07;
    break;
  case 1:
    _attribute |= 0x0808;
    break;
  case 22:
    _attribute &= ~0x0808;
    break;
  case 5:
    _attribute |= 0x8080;
    // FALLTHRU
  default:
    if (30 <= a && a <= 37)
      _attribute = (_attribute & 0x0f0) | colors[a - 30] | ((_attribute >> 8) & 0x08);
    else if (40 <= a && a <= 47)
      _attribute = (_attribute & 0x0f) | (colors[a - 40] << 4) | ((_attribute >> 8) & 0x80);
    break;
  };
}



PRIVATE
void Fb_console::esc_write(char const *str, size_t len, unsigned &i)
{
  if (i >= len)
    return;

  if (str[i] == '[')
    {
      ansi_esc_args[0] = 0;
      ansi_esc_args[1] = 0;
      num_ansi_esc_args = 0;
      wr = &Fb_console::ansi_esc_write;
    }
  else
    wr = &Fb_console::normal_write;

  ++i;
}

PRIVATE
void Fb_console::ansi_esc_write(char const *str, size_t len, unsigned &i)
{
  /* ANSI seq */
  for (; i < len && (isdigit(str[i]) || str[i] == ';'); ++i)
    {
      if (str[i] == ';')
        ansi_esc_args[++num_ansi_esc_args] = 0;
      else if (num_ansi_esc_args < MAX_ANSI_ESC_ARGS)
        ansi_esc_args[num_ansi_esc_args]
          = ansi_esc_args[num_ansi_esc_args] * 10 + (str[i] - '0');
    }

  if (i >= len)
    return;

  switch (str[i])
    {
  case 'f': /* Move cursor + blink cursor to location v,h */
    _x = ansi_esc_args[1];
    if (_y)
      --_y;
    _y = ansi_esc_args[0];
    if (_x)
      --_x;
    if (_x >= _screen.width)
      _x = _screen.width - 1;
    if (_y >= _screen.height)
      _y = _screen.height - 1;
    blink_cursor(_x, _y);
    break;

  case 'H': /* Move cursor to screen location v,h */
    _x = ansi_esc_args[1];
    if (_x)
      --_x;
    _y = ansi_esc_args[0];
    if (_y)
      --_y;
    if (_x >= _screen.width)
      _x = _screen.width - 1;
    if (_y >= _screen.height)
      _y = _screen.height - 1;
    break;

  case 'm': /*color*/
    {
      int p = 0;
      do {
        ansi_attrib(ansi_esc_args[p++]);
      } while (num_ansi_esc_args--);
    }
    break;

  case 'X': /* clear n characters */
    for (unsigned i = _x + (_y * _screen.width);
         i < _x + (_y * _screen.width) + ansi_esc_args[0]; ++i)
      set(i, 0x20, _attribute);

    break;

  case 'K': /* Clear line from cursor right */
    switch (ansi_esc_args[0])
    {
    default:
    case 0:
      for (unsigned i = _x + (_y * _screen.width); i < (_y * _screen.width) + _screen.width; ++i)
        set(i, 0x20, _attribute);
      break;

    case 1:
      for (unsigned i = (_y * _screen.width); i < (_y * _screen.width) + _x; ++i)
        set(i, 0x20, _attribute);
      break;

    case 2:
      for (unsigned i = (_y * _screen.width); i < (_y * _screen.width) + _screen.width; ++i)
        set(i, 0x20, _attribute);
      break;

    }
    break;

  case 'J': /* Clear screen from cursor */
    switch (ansi_esc_args[0])
    {
    default:
    case 0:
      for (unsigned i = (_y * _screen.width);
           i < (_y * _screen.width) + _screen.width * (_screen.height - _y); ++i)
        set(i, 0x20, _attribute);
      break;

    case 1:
      for (unsigned i = 0; i < _screen.width * _y; ++i)
        set(i, 0x20, _attribute);
      break;

    case 2:
      for (unsigned i = 0; i < _screen.width * _screen.height; ++i)
        set(i, 0x20, _attribute);
      break;
    }
    break;
  }

  wr = &Fb_console::normal_write;
  ++i;
}

PRIVATE
void Fb_console::normal_write(char const *str, size_t len, unsigned &i)
{
  for (; i < len; ++i)
    switch (str[i])
      {
      case '\n':
      case '\f':
        _x = 0;
        ++_y;
        if (_y >= _screen.height)
          {
            scroll(_y - _screen.height + _scroll_delta);
            _y = _screen.height - _scroll_delta;
          }
        break;

      case '\r':
        _x = 0;
        break;

      case '\t':
        _x = (_x & ~7) + 8;
        // TODO: wrap around
        break;

      case 27: /* ESC */
        wr = &Fb_console::esc_write;
        ++i;
        return;

      case 6: /* cursor */
        seq_6(str, len, i);
        break;

      case 1: /* home */
        seq_1(str, len, i);
        break;

      case 5: /* clear to end of line */
        seq_5(str, len, i);
        break;

      case 8: /* back space */
        if (_x)
          --_x;
        break;

      default:
        if (static_cast<unsigned>(str[i]) >= 32)
          {
            if (_x >= _screen.width)
              {
                ++_y;
                _x = 0;
              }

            if (_y >= _screen.height)
              {
                scroll(_y - _screen.height + _scroll_delta);
                _y = _screen.height - _scroll_delta;
              }

            if (_x < _screen.width)
              set(_y * _screen.width + _x,
                  str[i] == '\265' ? '\346' : str[i], _attribute);

            ++_x;
          }
        break;
      }
}


IMPLEMENT
int Fb_console::write( char const *str, size_t len )
{
  unsigned pos = 0;
  while (pos < len)
    (this->*wr)(str, len, pos);

  return pos;
}

IMPLEMENT
int Fb_console::getchar(bool)
{
  blink_cursor(_x, _y);
  return -1;
}

IMPLEMENT
Fb_console::~Fb_console()
{}

IMPLEMENT inline
bool Fb_console::is_working()
{
  return true;
}

PUBLIC
Mword Fb_console::get_attributes() const override
{
  return DIRECT | OUT;
}
