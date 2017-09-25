IMPLEMENTATION [ppc32]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}
