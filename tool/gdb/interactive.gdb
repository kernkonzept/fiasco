# gdb script for common interactive inspection use

set language c++
set pagination off
python
import os
sys.path.insert(0, os.environ['FIASCO_TOOL_GDB_DIR'])
import fiasco_gdb_util
end
