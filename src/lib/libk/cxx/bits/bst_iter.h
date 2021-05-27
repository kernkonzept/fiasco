// vi:ft=cpp
/**
 * \file
 * \brief AVL tree
 */
/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#pragma once

#include "bst_base.h"

namespace cxx { namespace Bits {

/**
 * \internal
 * \ingroup cxx_api
 * \brief Generic iterator for the AVL-tree based set.
 * \param Cmp the type of the comparison functor.
 * \param Node the type of a node.
 * \param Node_op the type used to determine the direction of the iterator.
 */
template< typename Node, typename Node_op >
class __Bst_iter_b
{
protected:
  typedef Direction Dir;
  Node const *_n;   ///< Current node
  Node const *_r;   ///< Root node of current subtree

  /// Create an invalid iterator, used as end marker.
  __Bst_iter_b() : _n(0), _r(0) {}

  /**
   * \brief Create an iterator for the given AVL tree.
   * \param t the root node of the tree to iterate.
   * \param cmp the comparison functor for tree elements.
   */
  __Bst_iter_b(Node const *t)
    : _n(t), _r(_n)
  { _downmost(); }

  __Bst_iter_b(Node const *t, Node const *r)
    : _n(t), _r(r)
  {}

  /// traverse the subtree down to the leftmost/rightmost leave.
  inline void _downmost();

  /// Increment to the next element.
  inline void inc();

public:
  /// Check two iterators for equality.
  bool operator == (__Bst_iter_b const &o) const { return _n == o._n; }
  /// Check two iterators for non equality.
  bool operator != (__Bst_iter_b const &o) const { return _n != o._n; }
};

/**
 * \internal
 * \ingroup cxx_api
 * \brief Generic iterator for the AVL-tree based set.
 * \param Node the type of a node.
 * \param Node_type the type of the node to return stored in a node.
 * \param Node_op the type used to determine the direction of the iterator.
 */
template< typename Node, typename Node_type, typename Node_op >
class __Bst_iter : public __Bst_iter_b<Node, Node_op>
{
private:
  /// Super-class type
  typedef __Bst_iter_b<Node, Node_op> Base;

  using Base::_n;
  using Base::_r;
  using Base::inc;

public:
  /// Create an invalid iterator (end marker)
  __Bst_iter() {}

  /**
   * \brief Create an iterator for the given tree.
   * \param t the root node of the tree to iterate.
   * \param cmp the conmparison functor for tree elements.
   */
  __Bst_iter(Node const *t) : Base(t) {}
  __Bst_iter(Node const *t, Node const *r) : Base(t, r) {}

//  template<typename Key2>
  __Bst_iter(Base const &o) : Base(o) {}

  /**
   * \brief Dereference the iterator and get the item out of the tree.
   * \return A reference to the data stored in the AVL tree.
   */
  Node_type &operator * () const { return *const_cast<Node *>(_n); }
  /**
   * \brief Member access to the item the iterator points to.
   * \return A pointer to the item in the node.
   */
  Node_type *operator -> () const { return const_cast<Node *>(_n); }
  /**
   * \brief Set the iterator to the next element (pre increment).
   */
  __Bst_iter &operator ++ () { inc(); return *this; }
  /**
   * \brief Set the iterator to the next element (post increment).
   */
  __Bst_iter &operator ++ (int)
  { __Bst_iter tmp = *this; inc(); return tmp; }
};


//----------------------------------------------------------------------------
/* IMPLEMENTATION: __Bst_iter_b */

template< typename Node, typename Node_op>
inline
void __Bst_iter_b<Node, Node_op>::_downmost()
{
  while (_n)
    {
      Node *n = Node_op::child(_n, Dir::L);
      if (n)
	_n = n;
      else
	return;
    }
}

template< typename Node, typename Node_op>
void __Bst_iter_b<Node, Node_op>::inc()
{
  if (!_n)
    return;

  if (_n == _r)
    {
      _r = _n = Node_op::child(_r, Dir::R);
      _downmost();
      return;
    }

  if (Node_op::child(_n, Dir::R))
    {
      _n = Node_op::child(_n, Dir::R);
      _downmost();
      return;
    }

  Node const *q = _r;
  Node const *p = _r;
  while (1)
    {
      if (Node_op::cmp(_n, q))
	{
	  p = q;
	  q = Node_op::child(q, Dir::L);
	}
      else if (_n == q || Node_op::child(q, Dir::R) == _n)
	{
	  _n = p;
	  return;
	}
      else
	q = Node_op::child(q, Dir::R);
    }
}

}}
