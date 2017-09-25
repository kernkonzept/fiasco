INTERFACE:

EXTENSION class L4_fpage
{
public:
  /**
   * task cap specific constants.
   */
  enum {
    Whole_obj_space = 25, ///< The order used to cover the whole x-cap space
    Obj_max    = 1L << Whole_obj_space, ///< Number of available task caps.
  };

  /**
   * Create the given task flex page.
   * @param port the port address.
   * @param order the size of the flex page is 2^order.
   * @param grant if not zero the grant bit is to be set.
   */
  static L4_fpage obj(Mword index, Mword order, Mword grant);

  /**
   * Get the x-cap flexpage base address.
   * @return The x-cap flexpage base address.
   */
  Mword obj() const;

  /**
   * Set the x-cap flexpage base address.
   * @param addr the x-cap flexpage base address.
   */
  void obj( Mword index );

  /**
   * Is the flex page a task-cap flex page?
   * @returns not zero if this flex page is a task-cap flex page.
   */
  Mword is_objpage() const;

  /**
   * Does the flex page cover the whole task-cap space.
   * @pre The is_cappage() method must return true or the 
   *      behavior is undefined.
   * @return not zero if the flex page covers the whole task-cap
   *          space.
   */
  Mword is_whole_obj_space() const;

private:
  enum {
    Obj_mask  = 0x007ff000,
    Obj_shift = 12,
  };
};

INTERFACE [32bit]:
  
EXTENSION class L4_fpage
{
private:
  enum 
  {
    Obj_id        = 0xf0000300,
  };
};

INTERFACE [64bit]:
  
EXTENSION class L4_fpage
{
private:
  enum 
  {
    Obj_id        = 0xfffffffff0000300UL,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [!caps]:

IMPLEMENT inline
Mword L4_fpage::obj() const
{
  return 0;
}

IMPLEMENT inline
Mword L4_fpage::is_objpage() const
{
  return 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [caps]:

IMPLEMENT inline
Mword L4_fpage::is_objpage() const
{ return (_raw & Special_fp_mask) == Obj_id; }

IMPLEMENT inline
void L4_fpage::obj( Mword w )
{
  _raw = (_raw & ~Obj_mask) | ((w << Obj_shift) & Obj_mask);
}

IMPLEMENT inline
Mword L4_fpage::obj() const
{
  return (_raw & Obj_mask) >> Obj_shift;
}

IMPLEMENT inline
L4_fpage L4_fpage::obj(Mword index, Mword order, Mword grant)
{
  return L4_fpage( (grant ? 1 : 0)
		   | ((index << Obj_shift) & Obj_mask)
		   | ((order << Size_shift) & Size_mask)
		   | Obj_id);
}

IMPLEMENT inline
Mword L4_fpage::is_whole_obj_space() const
{ return (_raw >> 2) == Whole_obj_space; }

