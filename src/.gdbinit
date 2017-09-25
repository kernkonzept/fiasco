define tcb
  p *(class thread_t*)((unsigned)&_tcbs_1 + (($arg0 << 17) + ($arg1 << 10) << 1))
end
define dtcb
  graph display *(class thread_t*)((unsigned)&_tcbs_1 + (($arg0 << 17) + ($arg1 << 10) << 1))
end

define tcbat
  p *(class thread_t*)($arg0)
end

file fiasco.image
set remotebaud 115200

## for debugging on COM2
target remote /dev/ttyS1

## for debugging on COM2
#target remote /dev/ttyS0

set output-radix 16
set history save on
