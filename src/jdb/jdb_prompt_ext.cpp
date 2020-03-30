INTERFACE:

#include <cxx/hlist>

class Jdb_prompt_ext : public cxx::H_list_item
{
public:
  Jdb_prompt_ext();
  virtual void ext() = 0;
  virtual void update();
  virtual ~Jdb_prompt_ext() = 0;

  static void do_all();
  static void update_all();

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

IMPLEMENT inline Jdb_prompt_ext::~Jdb_prompt_ext() {}

IMPLEMENT
void Jdb_prompt_ext::update()
{}

IMPLEMENT
void Jdb_prompt_ext::do_all()
{
  for (auto const &&e: exts)
    e->ext();
}

IMPLEMENT
void Jdb_prompt_ext::update_all()
{
  for (auto const &&e: exts)
    e->update();
}

