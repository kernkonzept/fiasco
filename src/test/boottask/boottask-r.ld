/* -*- c -*- */

SECTIONS
{
  /DISCARD/ :		     /* discard all sections not stripped by `strip' */
    {
      *(.comment .note)
    }
  .data :
    {
      boottask_start = .;
      *(*)
      boottask_end = .;
    }
}
