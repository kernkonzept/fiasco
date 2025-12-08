INTERFACE [dt]:

// Have to place this explicitly here because of some preprocess limitation...
extern "C" {
#include <libfdt.h>
}

// ------------------------------------------------------------------------
INTERFACE:

#include <stdint.h>

/**
 * Read-only wrapper around a flattened device tree.
 */
class Dt
{
public:
  class Node;

  /**
   * Optional return value to control callback-driven loops.
   */
  enum Cb
  {
    Break,
    Continue,
  };

  static void init();
  static void const *fdt();
  static bool have_fdt() { return fdt() != nullptr; }

  static Node node_by_path(char const *path, int namelen);
  static Node node_by_path(char const *path);
  static Node node_by_phandle(uint32_t phandle);
  static Node node_by_compatible(char const *compatible);

  template<typename CB>
  static void nodes_by_compatible(char const *compatible, CB &&cb);

  template<typename ARR>
  static Node node_by_compatible_list(ARR const &compatible);

  template<typename CB>
  static void nodes_by_prop_value(char const *name, void const *val, int len,
                                  CB &&cb);
};

// ------------------------------------------------------------------------
INTERFACE [!dt]:

EXTENSION class Dt
{
public:
  struct Array
  {
    constexpr bool is_present() const
    { return false; }

    template<typename T>
    constexpr T get(unsigned) const
    { return 0; }

    constexpr unsigned len() const
    { return 0; }

    constexpr Array view(unsigned, unsigned) const
    { return Array(); }
  };

  template<unsigned N>
  struct Array_prop
  {
    constexpr bool is_present() const
    { return false; }

    constexpr bool is_valid() const
    { return false; }

    template<typename T = uint64_t>
    constexpr T get(unsigned, unsigned) const
    { return 0; }

    constexpr unsigned elements() const { return 0; }
  };

  class Node
  {
    constexpr bool is_valid() const { return false; }
    constexpr operator bool() const { return false; }
    constexpr bool is_root_node() const { return false; }

    constexpr Node parent_node() const { return Node(); }

    constexpr char const *get_name(char const *default_name = nullptr) const
    { return default_name; }

    constexpr bool has_prop(char const *) const { return false; }

    constexpr bool get_prop_u32(char const *, uint32_t *) const
    { return false; }

    constexpr bool get_prop_u64(char const *, uint64_t *) const
    { return false; }

    constexpr uint32_t get_prop_default_u32(char const *, uint32_t dflt) const
    { return dflt; }

    constexpr uint64_t get_prop_default_u64(char const *, uint64_t dflt) const
    { return dflt; }

    constexpr char const *get_prop_str(char const *) const
    { return nullptr; }

    constexpr Array get_array(char const *) const
    { return Array(); }

    template<unsigned N>
    constexpr Array_prop<N> get_prop_array(char const *, unsigned const (&)[N]) const
    { return Array_prop<N>(); }

    template<typename CB>
    constexpr void for_each_reg(CB &&) const
    {}

    constexpr bool get_reg(unsigned, uint64_t *, uint64_t * = nullptr) const
    { return false; }

    constexpr bool get_reg_untranslated(unsigned, uint64_t *, uint64_t * = nullptr) const
    { return false; }

    constexpr bool check_compatible(char const *) const
    { return false; }

    constexpr bool check_device_type(char const *) const
    { return false; }

    constexpr bool is_enabled() const
    { return false; }

    template<typename CB>
    constexpr void for_each_subnode(CB &&) const
    {}

    template<typename CB>
    constexpr void for_each_phandle(char const *, char const *, CB &&) const
    {}
  };
};

// ------------------------------------------------------------------------
INTERFACE [dt]:

#include <cassert>
#include <cxx/type_traits>
#include <minmax.h>
#include <string.h>
#include <types.h>

#include "warn.h"

/**
 * Read-only wrapper around a flattened device tree.
 */
EXTENSION class Dt
{
public:
  /**
   * Accessor for array encoded properties.
   */
  class Array
  {
  public:
    Array()
      : _prop(nullptr), _len(0)
    {}

    Array(fdt32_t const *data, unsigned len)
      : _prop(data), _len(len)
    {}

    bool is_present() const
    { return _prop != nullptr; }

    template<typename T>
    T get(unsigned off) const
    {
      assert(off < _len);
      assert(off + (sizeof(T) / sizeof(fdt32_t)) <= _len);

      int len = min<unsigned>(sizeof(T) / sizeof(fdt32_t), _len - off);
      return read_value<T>(_prop + off, len);
    }

    unsigned len() const
    { return _len; }

    Array view(unsigned off, unsigned len) const
    {
      assert(off + len <= _len);
      return Array(_prop + off, len);
    }

  private:
    fdt32_t const *_prop;
    unsigned _len;
  };

  /**
   * Accessor for array properties with regularly sized cells.
   */
  template<unsigned N>
  class Array_prop
  {
  public:
    Array_prop()
      : _prop(nullptr), _len(0)
    {}

    Array_prop(fdt32_t const *prop, unsigned len, unsigned const cell_size[N])
      : _prop(prop), _len(len)
    {
      _elem_size = 0;
      for (unsigned i = 0; i < N; i++)
        {
          _cell_size[i] = cell_size[i];
          _elem_size += cell_size[i];
        }
    }

    bool is_present() const
    { return _prop != nullptr; }

    bool is_valid() const
    { return is_present() && (_len % _elem_size) == 0; }

    template<typename T = uint64_t>
    T get(unsigned element, unsigned cell) const
    {
      assert(cell < N);
      assert(element < elements());

      unsigned off = element * _elem_size;
      for (unsigned i = 0; i < cell; i++)
        off += _cell_size[i];

      return read_value<T>(_prop + off, _cell_size[cell]);
    }

    unsigned elements() const
    { return _len / _elem_size; }

  private:
    fdt32_t const *_prop;
    unsigned _len;
    unsigned _cell_size[N];
    unsigned _elem_size;
  };

  /**
   * Data and methods associated with a range property in a device tree.
   *
   * Ranges in a device tree describe the translation of regions from one
   * domain to another.
   */
  class Range
  {
  public:
    using Cell = uint64_t;

    /**
     * Translate an address from one domain to another.
     *
     * This function takes an address and a size and translates the address
     * from one domain to another if there is a matching range.
     *
     * \param[in,out] address  Address that shall be translated.
     * \param[in]     size     Size associated with the address.
     */
    bool translate(Cell *address, Cell const &size) const
    {
      if (match(*address, size))
        {
          *address = (*address - _child) + _parent;
          return true;
        }
      return false;
    }

    Range(Cell const &child, Cell const &parent, Cell const &length)
    : _child{child}, _parent{parent}, _length{length} {};

  private:
    // Child bus address
    Cell _child;
    // Parent bus address
    Cell _parent;
    // Range size
    Cell _length;

    /** Is [address, address + size] subset of [child, child + length]? */
    bool match(Cell const &address, Cell const &size) const
    {
      auto address_max = address + size;
      auto child_max = _child + _length;
      return _child <= address && address_max <= child_max;
    }
  };

  class Node
  {
    template<typename R>
    R const *get_prop(char const *name, int *size = nullptr) const
    {
      auto prop = static_cast<R const *>(
        fdt_getprop_namelen(Dt::fdt(), _off, name, strlen(name), size));

      if (prop && size)
        *size /= sizeof(R);

      return prop;
    }

    template<typename T>
    bool get_prop_val(char const *name, T *val) const
    {
      int len;
      auto prop = get_prop<fdt32_t>(name, &len);
      if (!prop)
        return false;

      *val = read_value<T>(prop, len);
      return true;
    }

    template<typename T>
    T get_prop_default(char const *name, T const &def) const
    {
      T ret;
      if (get_prop_val(name, &ret))
        return ret;
      else
        return def;
    }

  public:
    explicit Node() : _off(-1) {}
    explicit Node(int off) : _off(off) {}

    bool is_valid() const
    { return _off >= 0; }

    explicit operator bool() const
    { return is_valid(); }

    bool is_root_node() const
    { return _off == 0; }

    Node parent_node() const
    { return Node(fdt_parent_offset(Dt::fdt(), _off)); }

    char const *get_name(char const *default_name = nullptr) const
    {
      if (is_root_node())
        return "/";

      auto name = fdt_get_name(Dt::fdt(), _off, nullptr);
      return name ? name : default_name;
    }

    bool has_prop(char const *name) const
    {
      return fdt_getprop_namelen(Dt::fdt(), _off, name, strlen(name), nullptr);
    }

    bool get_prop_u32(char const *name, uint32_t *val) const
    { return get_prop_val(name, val); }

    bool get_prop_u64(char const *name, uint64_t *val) const
    { return get_prop_val(name, val); }

    uint32_t get_prop_default_u32(const char *name, uint32_t def) const
    { return get_prop_default<uint32_t>(name, def); }

    uint64_t get_prop_default_u64(const char *name, uint64_t def) const
    { return get_prop_default<uint64_t>(name, def); }

    char const *get_prop_str(char const *name) const
    {
      int len;
      char const *str = get_prop<char>(name, &len);
      // Ensure that the string is null-terminated as required by DTB spec.
      if (str && strnlen(str, len) == static_cast<size_t>(len))
        {
          WARN("Property '%s' does not hold a valid string.\n", name);
          return nullptr;
        }
      return str;
    }

    Array get_array(char const *name) const
    {
      int len;
      fdt32_t const *cells = get_prop<fdt32_t>(name, &len);
      return Array(cells, len);
    }

    template<unsigned N>
    Array_prop<N> get_prop_array(char const *name,
                                 unsigned const (&elems)[N]) const
    {
      int len;
      fdt32_t const *cells = get_prop<fdt32_t>(name, &len);
      if (cells)
        return Array_prop<N>(cells, len, elems);
      else
        return Array_prop<N>();
    }

    template<typename CB>
    void for_each_reg(CB &&cb) const
    {
      Node parent = parent_node();
      Reg_array_prop regs;
      if (!get_reg_array(parent, regs))
        return;

      for (unsigned i = 0; i < regs.elements(); i++)
        {
          uint64_t addr, size;
          if (get_reg_val(parent, regs, i, &addr, &size))
            if (invoke_cb(cb, addr, size) == Break)
              break;
        }
    }

    bool get_reg(unsigned index, uint64_t *addr,
                 uint64_t *size = nullptr) const
    {
      Node parent = parent_node();
      Reg_array_prop regs;
      if (!get_reg_array(parent, regs))
        return false;

      return get_reg_val(parent, regs, index, addr, size);
    }

    bool get_reg_untranslated(unsigned index, uint64_t *addr,
                              uint64_t *size = nullptr) const
    {
      Node parent = parent_node();
      Reg_array_prop regs;
      if (!get_reg_array(parent, regs))
        return false;

      if (index >= regs.elements())
        return false;

      if (addr)
        *addr = regs.get(index, 0);
      if (size)
        *size = regs.get(index, 1);

      return true;
    }

    bool check_compatible(char const *compatible) const
    { return fdt_node_check_compatible(Dt::fdt(), _off, compatible) == 0; }

    bool check_device_type(char const *device_type) const
    {
      char const *prop = get_prop_str("device_type");
      return prop && !strcmp(prop, device_type);
    }

    bool is_enabled() const
    {
      char const *status = get_prop_str("status");
      return !status || !strcmp("ok", status) || !strcmp("okay", status);
    }

    template<typename CB>
    void for_each_subnode(CB &&cb) const
    {
      int node;
      fdt_for_each_subnode(node, Dt::fdt(), _off)
        if (invoke_cb(cb, Node(node)) == Break)
          return;
    }

    /**
     * Iterate over a phandle list.
     *
     * \param list_name   Property name that contains the phandle list.
     * \param cells_name  Property name that specifies the argument count of a
     *                    phandle, i.e. the property is defined in each of the
     *                    referenced nodes.
     * \param cb          Callback invoked for each phandle, taking the
     *                    referenced node and the arguments as an Dt::Array.
     */
    template<typename CB>
    void for_each_phandle(char const *list_name,
                          char const *cells_name, CB &&cb) const
    {
      Array list = get_array(list_name);
      if (!list.is_present())
        return;

      unsigned cur_cell = 0;
      while (cur_cell < list.len())
        {
          uint32_t phandle = list.get<uint32_t>(cur_cell++);
          Node node = node_by_phandle(phandle);
          uint32_t arg_cells;
          if (!node.is_valid() || !node.get_prop_u32(cells_name, &arg_cells))
            // We have to abort the iteration if the referenced node is invalid
            // or if we cannot figure out the number of argument cells and
            // therefore cannot just skip the node.
            return;

          if (invoke_cb(cb, node, list.view(cur_cell, arg_cells)) == Break)
            return;

          cur_cell += arg_cells;
        }
    }

  private:
    using Reg_array_prop = Array_prop<2>;

    bool get_addr_size_cells(unsigned *addr_cells, unsigned *size_cells) const
    {
      int addr = fdt_address_cells(_fdt, _off);
      int size = fdt_size_cells(_fdt, _off);
      if (addr < 0 || addr > 2 || size < 0 || size > 2)
        return false;

      *addr_cells = addr;
      *size_cells = size;
      return true;
    }

    bool translate_reg(Node parent, uint64_t *addr, uint64_t const *size) const
    {
      if (parent.is_root_node())
        return true;

      unsigned addr_cells, size_cells;
      if (!parent.get_addr_size_cells(&addr_cells, &size_cells))
        return false;

      Node parent_parent = parent.parent_node();
      unsigned parent_addr_cells, parent_size_cells;
      if (!parent_parent.get_addr_size_cells(&parent_addr_cells, &parent_size_cells))
        return false;

      auto ranges = parent.get_prop_array("ranges", { addr_cells, parent_addr_cells,
                                                      size_cells });

      // No mapping between parent and child address space without ranges property.
      if (!ranges.is_present())
        return false; // no translation possible

      if (!ranges.is_valid())
        return false;

      // Empty ranges property?
      if (ranges.elements() == 0)
        return true; // identity mapping

      // Iterate over all the address space mappings
      for (unsigned i = 0; i < ranges.elements(); i++)
        {
          Range range{ranges.get(i, 0), ranges.get(i, 1), ranges.get(i, 2)};
          if (range.translate(addr, *size))
            return translate_reg(parent_parent, addr, size);
        }
      return false;
    }

    bool get_reg_array(Node parent, Reg_array_prop &regs) const
    {
      if (!parent.is_valid())
        return false;

      unsigned addr_cells, size_cells;
      if (!parent.get_addr_size_cells(&addr_cells, &size_cells))
        return false;

      regs = get_prop_array("reg", { addr_cells, size_cells });
      return regs.is_valid();
    }

    bool get_reg_val(Node parent, Reg_array_prop const &regs,
                     unsigned index, uint64_t *out_addr,
                     uint64_t *out_size) const
    {
      if (index >= regs.elements())
        return false;

      uint64_t addr = regs.get(index, 0);
      uint64_t size = regs.get(index, 1);
      if (!translate_reg(parent, &addr, &size))
        return false;

      if (out_addr)
        *out_addr = addr;
      if (out_size)
        *out_size = size;
      return true;
    }

    int _off;
  };

protected:
  template<typename F, typename... Args>
  inline static Cb invoke_cb(F f, Args... args)
  {
    if constexpr (cxx::is_same_v<void, decltype(f(args...))>)
      {
        f(args...);
        return Continue;
      }
    else
      return f(args...);
  }

  template<typename CB, typename NEXT>
  static void nodes_by(CB &&cb, NEXT &&next)
  {
    for (int node = next(-1); node >= 0; node = next(node))
      if (invoke_cb(cb, Node(node)) == Break)
        return;
  }

  template<typename T = uint64_t>
  static T read_value(fdt32_t const *cells, unsigned size)
  {
    uint64_t val = 0;
    for (unsigned i = 0; i < size; i++)
      val = (val << 32) | fdt32_to_cpu(cells[i]);

    if (size * sizeof(fdt32_t) > sizeof(T))
      WARN("Cell size %zu larger than provided storage type %zu!\n",
           size * sizeof(fdt32_t), sizeof(T));

    return static_cast<T>(val);
  }

  static void const *_fdt;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [!dt]:

IMPLEMENT static
void
Dt::init()
{}

IMPLEMENT static inline
void const *
Dt::fdt()
{ return nullptr; }

// ------------------------------------------------------------------------
IMPLEMENTATION [dt]:

#include "kip.h"
#include "kmem_mmio.h"
#include "panic.h"

void const *Dt::_fdt;

IMPLEMENT static
void
Dt::init()
{
  Address fdt_phys = Kip::k()->dt_addr;
  if (!fdt_phys)
    return;

  // Map the first page to determine the device tree size.
  void *fdt = Kmem_mmio::map(fdt_phys, sizeof(struct fdt_header),
                             Kmem_mmio::Map_attr::Cached());
  if (!fdt)
    return;

  int fdt_check = fdt_check_header(fdt);
  if (fdt_check < 0)
    {
      WARN("FDT sanity check failed: %s (%d)\n",
           fdt_strerror(fdt_check), fdt_check);
      Kmem_mmio::unmap(fdt, sizeof(struct fdt_header));
      return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  size_t fdt_size = fdt_totalsize(fdt);
#pragma GCC diagnostic pop

  Kmem_mmio::unmap(fdt, sizeof(struct fdt_header));

  // Finally map the entire device tree.
  _fdt = Kmem_mmio::map(fdt_phys, fdt_size, Kmem_mmio::Map_attr::Cached());
  if (!_fdt)
    panic("Cannot map FDT!");
}

IMPLEMENT static inline
void const *
Dt::fdt()
{ return _fdt; }

IMPLEMENT static
Dt::Node
Dt::node_by_path(char const *path, int namelen)
{
  if (!_fdt)
    return Node();

  return Node(fdt_path_offset_namelen(_fdt, path, namelen));
}

IMPLEMENT static
Dt::Node
Dt::node_by_path(char const *path)
{ return node_by_path(path, strlen(path)); }

IMPLEMENT static
Dt::Node Dt::node_by_phandle(uint32_t phandle)
{
  if (!_fdt)
    return Node();

  return Node(fdt_node_offset_by_phandle(_fdt, phandle));
}

IMPLEMENT static
Dt::Node
Dt::node_by_compatible(char const *compatible)
{
  if (!_fdt)
    return Node();

  return Node(fdt_node_offset_by_compatible(_fdt, -1, compatible));
}

IMPLEMENT template<typename CB> inline static
void
Dt::nodes_by_compatible(char const *compatible, CB &&cb)
{
  if (!_fdt)
    return;

  nodes_by(cb, [=](int node)
    { return fdt_node_offset_by_compatible(_fdt, node, compatible); });
}

IMPLEMENT template<typename ARR> inline static
Dt::Node
Dt::node_by_compatible_list(ARR const &compatible)
{
  if (!_fdt)
    return Node();

  for (int node = fdt_next_node(_fdt, -1, nullptr);
       node >= 0;
       node = fdt_next_node(_fdt, node, nullptr))
    {
      for (char const *i : compatible)
        {
          int err = fdt_node_check_compatible(_fdt, node, i);
          if (err < 0 && err != -FDT_ERR_NOTFOUND)
            return Node(err);
          else if (err == 0)
            return Node(node);
        }
    }

  return Node();
}

IMPLEMENT template<typename CB> inline static
void
Dt::nodes_by_prop_value(char const *name, void const *val, int len, CB &&cb)
{
  if (!_fdt)
    return;

  nodes_by(cb, [=](int node)
    { return fdt_node_offset_by_prop_value(_fdt, node, name, val, len); });
}
