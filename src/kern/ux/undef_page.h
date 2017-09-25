// undefine constants defined in <sys/user.h> since they would re-define
// variables in the config class
#undef PAGE_SHIFT
#undef PAGE_SIZE
#undef PAGE_MASK
