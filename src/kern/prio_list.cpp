INTERFACE:

#include <cassert>
#include <cxx/dlist>
#include <cxx/hlist>
#include "member_offs.h"
#include "spin_lock.h"
#include "types.h"

/**
 * Priority sorted list with insert complexity O(n) n = number of available
 * priorities (256 in Fiasco).
 */


class Prio_list_elem;

/**
 * Priority sorted list.
 *
 * The list is organized in a way that the highest priority member can be
 * found with O(1). Ever dequeue operation is also O(1).
 *
 * There is a forward iteratable list with at most one element per priority.
 * Elements with the same priority are handled in a double-linked circular
 * list for each priority. This double-linked list implements FIFO policy for
 * finding the next element.
 */
class Prio_list : private cxx::H_list<Prio_list_elem>
{
  MEMBER_OFFSET();
  friend class Jdb_sender_list;
public:
  typedef cxx::H_list<Prio_list_elem> P_list;
  typedef cxx::D_list_cyclic<Prio_list_elem> S_list;

  using P_list::front;
  using P_list::empty;

  Prio_list_elem *first() const { return front(); }
};

class Iteratable_prio_list : public Prio_list
{
public:
  Spin_lock<> *lock() { return &_lock; }

private:
  Prio_list_elem *_cursor;
  Spin_lock<> _lock;
};

typedef Iteratable_prio_list Locked_prio_list;


/**
 * Single element of a priority sorted list.
 */
class Prio_list_elem : public cxx::H_list_item, public cxx::D_list_item
{
  MEMBER_OFFSET();
private:
  friend class Prio_list;
  friend class Jdb_sender_list;
  typedef cxx::D_list_cyclic<Prio_list_elem> S_list;

  /**
   * Priority, the higher the better.
   */
  unsigned short _prio;
};



IMPLEMENTATION:

/**
 * Setup pointers for enqueue.
 */
PRIVATE inline
void
Prio_list_elem::init(unsigned short p)
{
  _prio = p;
}

/**
 * Get the priority.
 */
PUBLIC inline
unsigned short
Prio_list_elem::prio() const
{ return _prio; }

PUBLIC inline
Prio_list_elem *
Prio_list::next(Prio_list_elem *e) const
{
  S_list::Iterator i = ++S_list::iter(e);
  if (P_list::in_list(*i))
    return *++P_list::iter(*i);
  return *i;
}

/**
 * Insert a new element into the priority list.
 * @param e the element to insert
 * @param prio the priority for the element
 */
PUBLIC inline NEEDS[Prio_list_elem::init]
void
Prio_list::insert(Prio_list_elem *e, unsigned short prio)
{
  assert (e);
  e->init(prio);

  Iterator pos = begin();

  while (pos != end() && pos->prio() > prio)
    ++pos;

  if (pos != end() && pos->prio() == prio)
    S_list::insert_before(e, S_list::iter(*pos));
  else
    {
      S_list::self_insert(e);
      insert_before(e, pos);
    }
}

/**
 * Is the element actually enqueued?
 * @return true if the element is actaully enqueued in a list.
 */
PUBLIC inline
bool Prio_list_elem::in_list() const { return S_list::in_list(this); }

/**
 * Dequeue a given element from the list.
 * @param e the element to dequeue
 */
PUBLIC inline
void
Prio_list::dequeue(Prio_list_elem *e, Prio_list_elem **next = 0)
{
  if (P_list::in_list(e))
    {
      assert (S_list::in_list(e));
      // yes we are the head of our priority
      if (S_list::has_sibling(e))
	{
	  P_list::replace(e, *++S_list::iter(e));
	  if (next) *next = *++S_list::iter(e);
	}
      else
	{
	  if (next) *next = *++P_list::iter(e);
	  P_list::remove(e);
	}
    }
  else
    {
      if (next)
	{
	  if (P_list::in_list(*++S_list::iter(e))) // actually we are the last on our priority
	    *next = *++P_list::iter(e);
	  else
	    *next = *++S_list::iter(e);
	}
    }
  S_list::remove(e);
}

PUBLIC inline
Iteratable_prio_list::Iteratable_prio_list() : _cursor(0), _lock(Spin_lock<>::Unlocked) {}

/**
 * Dequeue a given element from the list.
 * @param e the element to dequeue
 */
PUBLIC inline NEEDS[Prio_list::dequeue]
void
Iteratable_prio_list::dequeue(Prio_list_elem *e)
{
  Prio_list_elem **c = 0;
  if (EXPECT_FALSE(_cursor != 0) && EXPECT_FALSE(_cursor == e))
    c = &_cursor;

  Prio_list::dequeue(e, c);
}

PUBLIC inline
void
Iteratable_prio_list::cursor(Prio_list_elem *e)
{ _cursor = e; }

PUBLIC inline
Prio_list_elem *
Iteratable_prio_list::cursor() const
{ return _cursor; }

