INTERFACE:

#include <acpi.h>

struct Acpi_gtdt : public Acpi_table_head
{
  enum Timer_flags : Unsigned32
  {
    Interrupt_edge   = 1 << 0, ///< edge-triggered, otherwise level
    Interrupt_low    = 1 << 1, ///< active-low, otherwise active-high
    Always_on        = 1 << 2,
  };

  Unsigned64 cnt_control_base;
  Unsigned32 _res0;
  Unsigned32 secure_el1_gsiv;
  Unsigned32 secure_el1_flags;
  Unsigned32 non_secure_el1_gsiv;
  Unsigned32 non_secure_el1_flags;
  Unsigned32 virtual_el1_gsiv;
  Unsigned32 virtual_el1_flags;
  Unsigned32 non_secure_el2_gsiv;
  Unsigned32 non_secure_el2_flags;
  // Fields below only valid for rev >= 2 (ACPI 5.1)
  Unsigned64 cnt_read_base;
  Unsigned32 platform_timer_count;
  Unsigned32 platform_timer_offset;
  // Fields below only valid for rev >= 3 (ACPI 6.3)
  Unsigned32 virtual_el2_gsiv;
  Unsigned32 virtual_el2_flags;
} __attribute__((packed));

struct Acpi_iort : public Acpi_table_head
{
  Unsigned32 node_count;
  Unsigned32 node_offset;
  Unsigned32 _res0;

  struct Node
  {
    enum Type : Unsigned8
    {
      Its = 0,
      Named_component = 1,
      Root_complex = 2,
      Smmu_v2 = 3,
      Smmu_v3 = 4,
      Pmcg = 5,
      Memory_range = 6,
    };

    Type type;
    Unsigned16 len;
    Unsigned8 rev;
    Unsigned32 ident;
    Unsigned32 mappings_num;
    Unsigned32 mappings_offset;
  } __attribute__((packed));

  struct Smmu_v3 : public Node
  {
    enum Flags : Unsigned32
    {
      Cohacc_override = 1 << 0, ///< SMMU accesses memory cache coherently
    };

    Unsigned64 base_addr;
    Unsigned32 flags;
    Unsigned32 _res0;
    Unsigned64 vatos_addr;
    Unsigned32 model;
    Unsigned32 gsiv_event;
    Unsigned32 gsiv_pri;
    Unsigned32 gsiv_gerr;
    Unsigned32 gsiv_sync;
    Unsigned32 proximity_domain;
    Unsigned32 device_id_mapping;
  } __attribute__((packed));

  class Iterator
  {
    friend struct Acpi_iort;

  public:
    Iterator operator++()
    {
      if (_remaining)
        {
          _remaining--;
          _node = offset_cast<Node const *>(_node, _node->len);
        }
      else
        _node = nullptr;

      return *this;
    }

    Node const *operator*() const { return _node; }
    Node const *operator->() const { return _node; }

    bool operator==(Iterator const &other) const
    { return other._node == _node; }
    bool operator!=(Iterator const &other) const
    { return other._node != _node; }

  private:
    Iterator() : _node(nullptr) {}
    Iterator(Node const *node, Unsigned32 num) : _node(node), _remaining(num) {}

    Node const *_node;
    Unsigned32 _remaining;
  };

  Iterator begin() const
  {
    return Iterator(offset_cast<Node const *>(this, node_offset), node_count);
  }
  Iterator end() const { return Iterator(); }

} __attribute__((packed));

