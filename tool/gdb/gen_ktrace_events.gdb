# gdb script to generate ktrace_events.h file

set language c++
set pagination off
python
import os
sys.path.insert(0, os.environ['FIASCO_TOOL_GDB_DIR'])
import fiasco_gdb_util
end

fiasco-gen-ktrace-events
quit
