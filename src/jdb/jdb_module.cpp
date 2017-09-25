INTERFACE:

#include <cstdarg>
#include <cxx/hlist>
#include <cxx/slist>

#include "initcalls.h"

class Jdb_category;

/**
 * Base class for any kernel debugger module.
 *
 * This class is the base for any module of the
 * modularized Jdb. Ths idea is that Jdb can be
 * extended by plugging in a new module, which
 * provides one ore more commands and their
 * implementations to Jdb.
 *
 * A new module can be created by
 * deriving from Jdb_module and providing the neccessary
 * methods (num_cmds, cmds, action, and maybe the
 * constructor). To plug the module into Jdb a static
 * instance of the module must be created, with
 * "INIT_PRIORITY(JDB_MODULE_INIT_PRIO)".
 *
 */
class Jdb_module : public cxx::H_list_item
{
public:

  /**
   * A Jdb command description.
   *
   * A Jdb_module provides an array of such Cmd
   * structures, where each structure describes
   * one command.
   */
  struct Cmd {
    /**
     * The unique ID of the command within the module.
     *
     * This ID is handed to action().
     */
    int id;

    /**
     * The short command.
     */
    char const *scmd;

    /**
     * The normal (long) command.
     */
    char const *cmd;

    /**
     * The command format (possible options).
     *
     * This format string is somewhat like a "scanf"
     * format. It may contain normal text interleaved with
     * input format descriptions (like \%c). After the Jdb core
     * recognized the command the format string is printed
     * up to the first format descriptor, then input according
     * to the given format is requested, after this input the
     * procedure is repeated until there are no more format
     * descriptors and action() is called.
     *
     * The values read via the format input are stored into
     * Cmd::argbuf, which must provide enough space for storing
     * all the data according to the whole format string.
     *
     * Format descriptors:\n
     * A format descriptor always starts with \%; the \% may be
     * followed by an unsigned decimal number (len argument) which
     * specifies the maximum number of characters to read.
     * The integer formats ('d', 'i', 'o', 'u', 'X', and 'x') may be prefixed
     * with one or two 'l's, for reading "long int" or "long long int".
     *
     * The integer read formats require an int, long int, or long long int
     * sized buffer according to the number of 'l' modifiers; for the others
     * see the respective format description.
     *
     * @li '\%' print the "\%" character
     * @li 'i' read an integer (optionally as hexadecimal number,
     *   if it starts with "0x" or as octal number, if it
     *   starts with "0")
     * @li 'p' read a pointer in hexadecimal format ('void*' buffer)
     * @li 'o' read an octal integer
     * @li 'X', 'x' read a hexadecimal integer
     * @li 'd' read a decimal (optionally signed) integer
     * @li 'u' read a decimal unsigned integer
     * @li 'c' read a character and continue immediately ('char'
     *         buffer)
     * @li 's' read a string (the len argument must be given)
     *         ('char[len]' buffer)
     *
     */
    char const *fmt;

    /**
     * The description of the command.
     *
     * The description of a command must contain the command
     * syntax itself and followed by "\\t" the description.
     * If more than one line is needed "\\n" can be used to switch
     * to a new line and "\\t" again to align the description.
     */
    char const *descr;

    /**
     * The buffer for the read arguments.
     *
     * This buffer is used to store the data read via the
     * format description (see Cmd::fmt).
     */
    void *argbuf;

    /**
     * Creates a Jdb command.
     * @param _id command ID (see Cmd::id)
     * @param _scmd short command (see Cmd::scmd)
     * @param _cmd long command (see Cmd::cmd)
     * @param _fmt input format (see Cmd::fmt)
     * @param _descr command description (see Cmd::descr)
     * @param _argbuf pointer to argument buffer (see Cmd::argbuf)
     */
  };

  /**
   * Possible return codes from action().
   *
   * The actual handler of the Jdb_module (action())
   * may return any value of this type.
   *
   */
  enum Action_code {

    /// Do nothing, wait for the next command.
    NOTHING = 0,

    /// Leave the kernel debugger
    LEAVE,

    /// got KEY_HOME
    GO_BACK,

    /// there was an error (abort or invalid input)
    ERROR,

    /**
     * Wait for new input arguments
     * @see action() for detailed information.
     */
    EXTRA_INPUT,

    /**
     * Wait for new input arguments and interpret character
     *        in next_char as next keystroke
     * @see action() for detailed information.
     */
    EXTRA_INPUT_WITH_NEXTCHAR,
  };

  typedef void (Gotkey)(char *&str, int maxlen, int c);

  /**
   * Create a new instance of an Jdb_module.
   * @param category the name of the category the module
   *        fits in. This category must exist (see
   *        Jdb_category) or the module is added to the
   *        "MISC" category.
   *
   * This constructor automatically registers the module at the
   * Jdb_core. The derived modules must provide an own constructor
   * if another category than "MISC" should be used.
   *
   * @see Jdb_core
   * @see Jdb_category
   *
   */
  Jdb_module(char const *category = "MISC") FIASCO_INIT;

  /// dtor
  virtual ~Jdb_module() = 0;

  /**
   * The actual handler of the module.
   * @param cmd the command ID (see Cmd::id) of the executed command.
   * @param args a reference to the argument buffer pointer.
   * @param fmt a reference to the format string pointer.
   *
   * This method is pure virtual and must be provided by the
   * specific derivate of the Jdb_module. action() is called
   * if one of the module's commands was issued and the input
   * according to the format string is read.
   *
   * The args and fmt arguments are references because they may
   * be modified by the action() method and extra input may be
   * requested by returning Action_code::EXTRA_INPUT. In the
   * case where Action_code::EXTRA_INPUT is returned the Jdb_core
   * reads again the values according to the given format (fmt)
   * and enters action(). With this mechanism it is possible to
   * request further input depending on the already given input.
   *
   */
  virtual Action_code action(int cmd, void *&args, char const* &fmt,
			     int &next_char) = 0;

  /**
   * The number of commands this modules provides.
   *
   * This method must return how many Cmd structures can be
   * found in the array returned by cmds().
   *
   * @see cmds()
   *
   */
  virtual int num_cmds() const = 0;

  /**
   * The commands this module provides.
   *
   * An array of Cmd structures must be returned,
   * where each entry describes a single command.
   * The command IDs (see Cmd::id) should be unique
   * within the module, so that action() can distinguish
   * between the different commands.
   *
   * @see num_cmds()
   * @see Cmd
   * @see action()
   */
  virtual Cmd const * cmds() const = 0;

  /**
   * Get the category of this module.
   */
  Jdb_category const *category() const;

  /**
   * Get Cmd structure according to cmd.
   * @param cmd the command you are looking for.
   * @param short_mode if true the short commands are looked up
   *        (see Cmd::scmd).
   * @return A pointer to the Cmd structure if the command is
   *         found, or a null pointer otherwise.
   */
  Cmd const* has_cmd( char const* cmd, bool short_mode = false,
                      bool full = true) const;

  typedef cxx::H_list_bss<Jdb_module> List;
  static List modules;

private:
  Jdb_category const *_cat;
};

/**
 * A category that may contain some Jdb_modules.
 *
 * Each registered Jdb_module must be a member of one
 * category. The help-module Help_m uses this categories
 * for displaying sorted help.
 *
 */
class Jdb_category : public cxx::S_list_item
{
public:
  /**
   * Create a new category.
   * @param name the name of the new category, also used
   *        at Jdb_module creation (see Jdb_module::Jdb_module())
   * @param desc the short description of this category.
   * @param order the ordering number of the category.
   */
  Jdb_category(char const *name, char const *desc,
               unsigned order = 0 ) FIASCO_INIT;

  /**
   * Get the name of this category.
   */
  char const * name() const;

  /**
   * Get the description of this category.
   */
  char const * description() const;

public:

  /**
   * Look for the category with the given name.
   * @param name the name of the category you are lokking for.
   * @param _default if set to true the default ("MISC") 
   *        category is returned if no category with the given 
   *        name is found.
   *
   */
  static Jdb_category *find(char const *name, bool _default = false);

  typedef cxx::S_list_bss<Jdb_category> List;
  static List categories;

private:
  char const *const _name;
  char const *const _desc;
  unsigned const _order;
};


IMPLEMENTATION:

#include <cassert>
#include <cstring>

#include "static_init.h"

Jdb_category::List Jdb_category::categories;
Jdb_module::List Jdb_module::modules;

static Jdb_category INIT_PRIORITY(JDB_CATEGORY_INIT_PRIO)
  misc_cat("MISC", "misc debugger commands", 2000);

IMPLEMENT
Jdb_category::Jdb_category(char const *const name,
			   char const *const desc,
			   unsigned order)
  : _name(name), _desc(desc), _order(order)
{
  List::Iterator c = categories.begin();

  for (; c != categories.end(); ++c)
    if (c->_order >= order)
      break;
  categories.insert_before(this, c);
}

IMPLEMENT inline
char const * Jdb_category::name() const
{
  return _name;
}

IMPLEMENT inline
char const * Jdb_category::description() const
{
  return _desc;
}

IMPLEMENT
Jdb_category *Jdb_category::find(char const* name, bool _default)
{
  List::Const_iterator a;
  for (a = categories.begin();
       a != categories.end() && strcmp(a->name(), name) != 0; ++a)
    ;

  if (_default && a == categories.end())
    return &misc_cat;

  return *a;
}


IMPLEMENT
Jdb_module::Jdb_module(char const *category)
  : _cat(Jdb_category::find(category, true))
{
  modules.push_front(this);
}

IMPLEMENT inline Jdb_module::~Jdb_module() {}

PUBLIC static
bool
Jdb_module::match(char const *cmd, char const *match, bool full = true)
{
  if (!cmd || !*cmd || !match)
    return false;

  while (*cmd && *match && *match != ' ')
    {
      if (*cmd != *match)
	return false;

      ++cmd;
      ++match;
    }

  if ((!*match || *match == ' ') && !*cmd)
    return true;

  if (!full && !*match)
    return true;

  return false;
}

IMPLEMENT
Jdb_module::Cmd const*
Jdb_module::has_cmd(char const* cmd, bool short_mode, bool full) const
{
  int n = num_cmds();
  Cmd const* cs = cmds();
  for (int i = 0; i < n; ++i)
    {
      if (short_mode)
	{
	  if (match(cs[i].scmd, cmd))
	    return cs + i;
	}
      else
	{
	  if (match(cs[i].cmd, cmd, full))
	    return cs + i;
	}
    }

  return 0;
}

IMPLEMENT inline
Jdb_category const *
Jdb_module::category() const
{ return _cat; }

