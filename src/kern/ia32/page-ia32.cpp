INTERFACE [ia32 || amd64]:

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    // PAT:7 Dirty:6 Accessed:5 PCD:4 PWT:3 U/S:2 R/W:1 Present:0

    MAX_ATTRIBS   = 0x00000006,
    Cache_mask    = 0x00000018, ///< Cache attribute mask
    CACHEABLE     = 0x00000000, ///< PAT=0, PCD=0, PWT=0: PAT0 (WB)
    BUFFERED      = 0x00000010, ///< PAT=0, PCD=1, PWT=0: PAT2 (WC)
    NONCACHEABLE  = 0x00000018, ///< PAT=0, PCD=1, PWT=1: PAT3 (UC)
  };
};
