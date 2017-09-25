
INTERFACE:

#include <cstdio>			// for FILE *
#include "types.h"

class Multiboot_module;

class Loader
{
private:
  static unsigned int phys_base;
  static char const *errors[];

public:
  static FILE *open_module (const char * const path);

  static char const * load_module (const char * const path,
				   Multiboot_module *module,
				   unsigned long int memsize,
                                   bool quiet,
                                   unsigned long *start,
                                   unsigned long *end);

  static char const * copy_module (const char * const path,
				   Multiboot_module *module,
				   Address *load_addr,
				   bool quiet);
};

IMPLEMENTATION:

#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "initcalls.h"
#include "mem_layout.h"
#include "multiboot.h"

unsigned int Loader::phys_base = Mem_layout::Physmem;

char const *Loader::errors[] FIASCO_INITDATA =
{
  "Failed to open file",
  "Failed to load ELF header",
  "Failed ELF magic check",
  "Failed ELF version check",
  "Failed to load program header",
  "Failed to fit program section into memory",
  "Failed to load program section from file",
  "Failed to read module, out of memory?"
};

IMPLEMENT FIASCO_INIT
FILE *
Loader::open_module (const char * const path)
{
  char magic[3];		// 2 + 1 for the terminating \0
  FILE *fp;

  if ((fp = fopen (path, "r")) == NULL)
    return NULL;

  fgets (magic, sizeof (magic), fp);

  if (magic[0] == '\037' && magic[1] == '\213')		// GZIP
    {
      FILE *pp;
      char pipecmd[256];
      int c;

      fclose (fp);

      snprintf (pipecmd, sizeof (pipecmd), "zcat %s", path);

      if ((pp = popen (pipecmd, "r")) == NULL)
        return NULL;

      if ((fp = tmpfile()) == NULL)
        {
          pclose (pp);
          return NULL;
        }

      while ((c = fgetc (pp)) != EOF)
        fputc (c, fp);

      pclose (pp);
    }

  rewind (fp);

  return fp;
}

IMPLEMENT FIASCO_INIT
char const *
Loader::load_module (const char * const path,
                     Multiboot_module *module,
                     unsigned long int memsize,
                     bool quiet,
                     unsigned long *start,
                     unsigned long *end)
{
  FILE *fp;
  Elf32_Ehdr eh;
  Elf32_Phdr ph;
  long offset;
  int i;

  if ((fp = open_module (path)) == NULL)
    return errors[0];

  // Load ELF Header
  if (fread (&eh, sizeof (eh), 1, fp) != 1)
    {
      fclose (fp);
      return errors[1];
    }

  // Check if valid ELF magic, 32bit, little endian
  if (memcmp(eh.e_ident, ELFMAG, 4) != 0 ||
      eh.e_ident[EI_CLASS] != ELFCLASS32  ||
      eh.e_ident[EI_DATA]  != ELFDATA2LSB)
    {
      fclose (fp);
      return errors[2];
    }

  // Check if executable, i386 format, current ELF version
  if (eh.e_type    != ET_EXEC ||
      eh.e_machine != EM_386  ||
      eh.e_version != EV_CURRENT)
    {
      fclose (fp);
      return errors[3];
    }

  // Record entry point (initial EIP)
  module->reserved  = eh.e_entry;
  *start            = 0xffffffff;
  *end              = 0;

  // Load all program sections
  for (i = 0, offset = eh.e_phoff; i < eh.e_phnum; i++) {

    // Load Program Header
    if (fseek (fp, offset, SEEK_SET) == -1 ||
        fread (&ph, sizeof (ph), 1, fp) != 1)
      {
        fclose (fp);
        return errors[4];
      }

    offset += sizeof (ph);

    // Only consider non-empty load-sections
    if (ph.p_type != PT_LOAD || !ph.p_memsz)
      continue;

    // Check if section fits into memory
    if (ph.p_paddr + ph.p_memsz > memsize)
      {
        fclose (fp);
        return errors[5];
      }

    // Load Section with non-zero filesize
    if (ph.p_filesz && (fseek (fp, ph.p_offset, SEEK_SET) == -1 ||
        fread ((void *)(phys_base + ph.p_paddr), ph.p_filesz, 1, fp) != 1))
      {
        fclose (fp);
        return errors[6];
      }

    // Zero-pad uninitialized data if filesize < memsize
    if (ph.p_filesz < ph.p_memsz)
      memset ((void *)(phys_base + ph.p_paddr + ph.p_filesz), 0,
               ph.p_memsz - ph.p_filesz);

    if (ph.p_paddr < *start)
      *start = ph.p_paddr;
    if (ph.p_paddr + ph.p_memsz > *end)
      *end   = ph.p_paddr + ph.p_memsz;
  }

  if (! quiet)
    printf ("Loading Module 0x" L4_PTR_FMT "-0x" L4_PTR_FMT " [%s]\n",
	    (Address)*start, (Address)*end, path);

  fclose (fp);
  return 0;
}

IMPLEMENT FIASCO_INIT
char const *
Loader::copy_module (const char * const path,
		     Multiboot_module *module,
                     Address *load_addr,
		     bool quiet)
{
  FILE *fp;
  struct stat s;

  if ((fp = open_module (path)) == NULL)
    return errors[0];

  if (fstat (fileno (fp), &s) == -1)
    {
      fclose (fp);
      return errors[0];
    }

  *load_addr -= s.st_size;
  *load_addr &= Config::PAGE_MASK;	// this may not be necessary

  if (fread ((void *)(phys_base + *load_addr), s.st_size, 1, fp) != 1)
    {
      fclose (fp);
      return errors[7];
    }

  module->mod_start = *load_addr;
  module->mod_end   = *load_addr + s.st_size;

  if (! quiet)
    printf ("Copying Module 0x" L4_PTR_FMT "-0x" L4_PTR_FMT " [%s]\n",
	    (Address)module->mod_start, (Address)module->mod_end, path);

  fclose (fp);
  return 0;
}
