INTERFACE:

#include <cxx/hlist>

class Jdb_prompt_ext : public cxx::H_list_item
{
public:
  Jdb_prompt_ext();
  virtual void ext() = 0;

  static void do_all();

private:
  typedef cxx::H_list_bss<Jdb_prompt_ext> List;
  static List exts;
};

IMPLEMENTATION:

Jdb_prompt_ext::List Jdb_prompt_ext::exts;

IMPLEMENT
Jdb_prompt_ext::Jdb_prompt_ext()
{
  exts.push_front(this);
}

IMPLEMENT
void Jdb_prompt_ext::do_all()
{
  for (auto const &&e: exts)
    e->ext();
}
