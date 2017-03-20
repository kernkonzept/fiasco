IMPLEMENTATION [arm && pf_rcar3]:

void __attribute__ ((noreturn))
platform_reset(void)
{
  Platform_control::system_reset();

  for (;;)
    ;
}
