IMPLEMENTATION:

/**
 * Handle int3 extensions in the current thread's context. All functions
 * for which we don't need/want to switch to the debugging stack.
 * \return 0 if this function should be handled in the context of Jdb
 *         1 successfully handled
 */
PRIVATE static inline
int
Jdb::handle_int3_threadctx(Trap_state *ts)
{
  return handle_int3_threadctx_generic(ts);
}
