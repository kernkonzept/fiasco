// --------------------------------------------------------------------------
INTERFACE [arm && mptimer && omap4_pandaboard]:

EXTENSION class Timer
{
private:
  static Mword interval() { return 499999; }
};
