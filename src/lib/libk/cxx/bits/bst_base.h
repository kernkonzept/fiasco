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

/*
 * This file contains very basic bits for implementing binary serach trees
 */
namespace cxx {
/**
 * \brief Internal helpers for the cxx package.
 */
namespace Bits {

/**
 * \brief The direction to go in a binary search tree.
 */
struct Direction
{
  /// The literal direction values.
  enum Direction_e
  {
    L = 0, ///< Go to the left child
    R = 1, ///< Go to the right child
    N = 2  ///< Stop
  };
  unsigned char d;

  /// Uninitialized direction
  Direction() = default;

  /// Convert a literal direction (#L, #R, #N) to an object
  Direction(Direction_e d) : d(d) {}

  /// Convert a boolean to a direction (false == #L, true == #R)
  explicit Direction(bool b) : d(Direction_e(b)) /*d(b ? R : L)*/ {}

  /**
   * \brief Negate the direction.
   * \note This is only defined for a current value of #L or #R
   */
  Direction operator ! () const { return Direction(!d); }

  /// \name Comparison operators (equality and inequality)
  //@{
  bool operator == (Direction_e o) const { return d == o; }
  bool operator != (Direction_e o) const { return d != o; }
  bool operator == (Direction o) const { return d == o.d; }
  bool operator != (Direction o) const { return d != o.d; }
  //@}
};

/**
 * \brief Basic type of a node in a binary search tree (BST).
 */
class Bst_node
{
  // all BSTs are friends
  template< typename Node, typename Get_key, typename Compare >
  friend class Bst;

protected:
  /**
   * \name Access to BST linkage.
   *
   * Provide access to the tree linkage to inherited classes
   * Inherited nodes, such as AVL nodes should make these methods
   * private via 'using'
   */
  /*@{*/

  /// Get next node in direction \a d.
  static Bst_node *next(Bst_node const *p, Direction d)
  { return p->_c[d.d]; }

  /// Set next node of \a p in direction \a d to \a n.
  static void next(Bst_node *p, Direction d, Bst_node *n)
  { p->_c[d.d] = n; }

  /// Get pointer to link in direction \a d.
  static Bst_node **next_p(Bst_node *p, Direction d)
  { return &p->_c[d.d]; }

  /// Get next node in direction \a d as type \a Node.
  template< typename Node > static
  Node *next(Bst_node const *p, Direction d)
  { return static_cast<Node *>(p->_c[d.d]); }

  /// Rotate subtree \a t in the opposite direction of \a idir.
  static void rotate(Bst_node **t, Direction idir);
  /*@}*/

private:
  Bst_node *_c[2];

protected:
  /// Create uninitialized node
  Bst_node() {}

  /// Create initialized node
  explicit Bst_node(bool) { _c[0] = _c[1] = 0; }
};

inline
void
Bst_node::rotate(Bst_node **t, Direction idir)
{
  Bst_node *tmp = *t;
  *t = next(tmp, idir);
  next(tmp, idir, next(*t, !idir));
  next(*t, !idir, tmp);
}

}}
