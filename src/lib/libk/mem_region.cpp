INTERFACE:

class Mem_region
{
public:
  unsigned long start, end;

  Mem_region() : start(0), end(0) {}

  Mem_region(unsigned long start, unsigned long end) : start(start), end(end) {}

  bool valid() const { return start < end; }

  bool operator < (Mem_region const &o) const
  { return end < o.start; }

  bool overlaps(Mem_region const &o) const
  { return !(o < *this || *this < o); }

  bool contains(Mem_region const &o) const
  { return start <= o.start && end >= o.end; }

  void merge(Mem_region const &r)
  {
    start = start < r.start ? start : r.start;
    end   = end > r.end     ? end   : r.end;
  }

  unsigned long size() const
  { return end - start + 1; }

};


class Mem_region_map_base
{
public:
  virtual bool add(Mem_region const &r) = 0;
  virtual bool sub(Mem_region const &r) = 0;
};

template< unsigned E >
class Mem_region_map : public Mem_region_map_base
{
public:
  enum { Entries = E };
  Mem_region_map() : _l(0) {}

private:
  unsigned _l;

  Mem_region _r[Entries];
};


//--------------------------------------------------------------------------
IMPLEMENTATION:

PRIVATE template< unsigned E >
inline
void
Mem_region_map<E>::del(unsigned start, unsigned end)
{
  unsigned delta = end - start;
  for (unsigned p = start; p < end; ++p)
    _r[p] = _r[p + delta];

  _l -= delta;
}

PUBLIC template< unsigned E >
inline NEEDS[Mem_region_map::del]
bool
Mem_region_map<E>::add(Mem_region const &r)
{
  if (!r.valid())
    return true;

  unsigned pos = 0;
  for (;pos < _l && _r[pos] < r; ++pos) ;
  if (_l > pos && !(r < _r[pos]))
    { // overlap -> merge
      _r[pos].merge(r);
      for (unsigned p = pos + 1; p < _l; ++p)
	{
	  if (!(_r[pos] < _r[p]))
	    _r[pos].merge(_r[p]);
	  else
	    {
	      del(pos + 1, p);
	      return true;
	    }
	}
      _l = pos + 1;
      return true;
    }

  if (_l >= Entries)
    return false;

  for (unsigned p = _l; p > pos; --p) _r[p] = _r[p-1];
  ++_l;
  _r[pos] = r;
  return true;
}


PUBLIC template< unsigned E >
inline NEEDS[Mem_region_map::del]
bool
Mem_region_map<E>::sub(Mem_region const &r)
{
  if (!r.valid())
    return true;

  unsigned pos;
  for (pos = 0; pos < _l; ++pos)
    {
      if (_r[pos].overlaps(r))
	{
	  if (r.contains(_r[pos]))
	    {
	      del(pos, pos+1);
	      --pos; // ensure we do not skip the next element
	    }
	  else if (r.start <= _r[pos].start)
	    _r[pos].start = r.end + 1;
	  else if (r.end >= _r[pos].end)
	    _r[pos].end = r.start - 1;
	  else
	    {
	      if (_l >= Entries)
		return false;

	      for (unsigned p = _l; p > pos; --p) _r[p] = _r[p-1];
	      ++_l;
	      _r[pos+1].start = r.end + 1;
	      _r[pos+1].end = _r[pos].end;
	      _r[pos].end = r.start - 1;
	    }
	}
    }
  return true;
}

PUBLIC template< unsigned E >
inline
unsigned
Mem_region_map<E>::length() const
{ return _l; }

PUBLIC template< unsigned E >
inline
Mem_region const &
Mem_region_map<E>::operator [] (unsigned idx) const
{ return _r[idx]; }

PUBLIC template< unsigned E >
inline
Mem_region &
Mem_region_map<E>::operator [] (unsigned idx)
{ return _r[idx]; }
