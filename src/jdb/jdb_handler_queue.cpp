INTERFACE:

// XXX If there is a constructor needed for this class make sure that it is
//     called with a priority higher than JDB_MODULE_INIT_PRIO!

class Jdb_handler
{
  friend class Jdb_handler_queue;
  
public:
  Jdb_handler( void (*handler)() ) : handler(handler), next(0) {}
  void execute() { handler(); }

private:
  void (*handler)();
  Jdb_handler *next;
};

class Jdb_handler_queue
{
private:
  Jdb_handler *first;
};

IMPLEMENTATION:

PUBLIC inline
void
Jdb_handler_queue::add (Jdb_handler *handler)
{
  handler->next = first;
  first = handler;
}

PUBLIC
void
Jdb_handler_queue::execute () const
{
  Jdb_handler *h = first;
  while(h)
    {
      h->execute();
      h = h->next;
    }
}

