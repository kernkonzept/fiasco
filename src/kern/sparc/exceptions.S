.text
.global kern_kdebug_entry
kern_kdebug_entry:
  ta 0

.global vcpu_resume
vcpu_resume:
  ta 0

.global leave_by_trigger_exception
leave_by_trigger_exception:
  ta 0

.global leave_and_kill_myself
leave_and_kill_myself:
  ta 0
