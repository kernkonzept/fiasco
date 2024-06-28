INTERFACE [ia32 || amd64 || ux]:

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    MAX_ATTRIBS   = 0x00000006,
    Cache_mask    = 0x00000018, ///< Cache attribute mask
    CACHEABLE     = 0x00000000, ///< PAT=0, PCD=0, PWT=0: PAT0 (WB)
    BUFFERED      = 0x00000010, ///< PAT=0, PCD=1, PWT=0: PAT2 (WC)
    NONCACHEABLE  = 0x00000018, ///< PAT=0, PCD=1, PWT=1: PAT3 (UC)
  };
};
