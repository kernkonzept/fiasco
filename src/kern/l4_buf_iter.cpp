INTERFACE:

#include <types.h>
#include <l4_msg_item.h>
#include <l4_fpage.h>
#include <l4_types.h>

class L4_buf_iter
{
public:
  struct Item
  {
    L4_msg_item b;
    Mword d;

    Item() : b(0)
#ifndef NDEBUG
	     , d(0)
#endif
    {}
  };

  explicit L4_buf_iter(Utcb const *utcb, unsigned start)
  : _buf(&utcb->buffers[start]), _max(&utcb->buffers[Utcb::Max_buffers])
  { next(); }
  bool more() const { return _buf < _max; }
  Item const *get() const { return &c; }
  bool next();

private:
  Mword const *_buf;
  Mword const *const _max;
  Item c;
};

class L4_snd_item_iter
{
public:
  struct Item
  {
    L4_msg_item b;
    Mword d;

    Item()
    : b(0)
#ifndef NDEBUG
      , d(0)
#endif
    {}
  };

  explicit L4_snd_item_iter(Utcb const *utcb, unsigned offset)
  : _buf(&utcb->values[offset]),
    _max(&utcb->values[Utcb::Max_words]) {}
  bool more() const { return _buf < _max; }
  Item const *get() const { return &c; }
  bool next();

private:
  Mword const *_buf;
  Mword const *const _max;
  Item c;
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT inline
bool
L4_buf_iter::next()
{
  c.b = L4_msg_item(_buf[0]);
  if (EXPECT_FALSE(c.b.is_void()))
    return false;

  if (c.b.type() == L4_msg_item::Map && c.b.is_small_obj())
    c.d = c.b.get_small_buf().raw();
  else
    {
      ++_buf;
      if (EXPECT_FALSE(_buf >= _max))
	{
	  c.b = L4_msg_item(0);
	  return false;
	}

      c.d = _buf[0];
    }
  ++_buf;
  return true;
}


IMPLEMENT inline
bool
L4_snd_item_iter::next()
{
  c.b = L4_msg_item(_buf[0]);
  c.d = 0;

  ++_buf;

  if (EXPECT_FALSE(c.b.is_void()))
    return true;

  if (EXPECT_FALSE(_buf >= _max))
    return false;

  c.d = _buf[0];

  ++_buf;
  return true;
}
