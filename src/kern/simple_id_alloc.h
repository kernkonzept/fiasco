#pragma once

#include <bitmap.h>
#include "boot_alloc.h"
#include <warn.h>

/**
 * Cooperative ID allocator, i.e. the allocator has no automatic rollover
 * mechanism. So if all IDs are allocated, further ID allocations will fail and
 * the user of the allocator must either accept this or is responsible to free
 * some IDs first.
 *
 * \tparam Id          Type used to store IDs.
 * \tparam First_id    First ID that can be allocated, the ones below are
 *                     considered reserved.
 * \tparam Max_nr_id   The maximum number of IDs the allocator provides.
 * \tparam Invalid_id  Value that represents an invalid ID.
 */
template<typename Id, Id First_id, Id Max_nr_id, Id Invalid_id>
class Simple_id_alloc
{
public:
  explicit Simple_id_alloc(Id max_id)
  : _max_id(max_id),
    _free_ids(new Boot_object<Id_map>())
  {}

  Simple_id_alloc(Simple_id_alloc const &) = delete;
  Simple_id_alloc operator = (Simple_id_alloc const &) = delete;

  /**
   * Allocate a new ID.
   *
   * \return Allocated ID, or `Invalid_id` if allocation failed.
   */
  Id alloc_id()
  {
    for (Id id = First_id; id < _max_id; ++id)
      if (_free_ids->atomic_get_and_set_if_unset(id) == false)
        return id;

    // No ID left.
    return Invalid_id;
  }

  /**
   * Free previously allocated ID.
   *
   * \pre Must only be called with a valid ID, i.e. an ID returned by
   *      alloc_id() which has not already been freed again.
   *
   * \param id  The ID to free.
   */
  void free_id(Id id)
  {
    if (id >= Max_nr_id)
      {
        WARN("Attempt to free invalid ID.\n");
        return;
      }

    if (_free_ids->atomic_get_and_clear(id) != true)
      WARN("Freeing free ID.\n");
  }

  /// Get the ID stored in `id_storage`.
  static Id get_id(Id const *id_storage)
  {
    return access_once(id_storage);
  }

  /**
   * Get valid ID or allocate a new ID.
   *
   * \param[in,out] id_storage  ID storage to get ID from, or if not valid,
   *                            to allocate a new ID for.
   *
   * \return The ID stored in `id_storage`, if valid, otherwise a newly
   *         allocated ID, or `Invalid_id` if allocation failed.
   */
  Id get_or_alloc_id(Id *id_storage)
  {
    Id id = access_once(id_storage);
    if (id != Invalid_id)
      return id;

    // Otherwise try to allocate an ID.
    Id new_id = alloc_id();
    if (EXPECT_FALSE(new_id == Invalid_id))
      return Invalid_id;

    if (!cas<Id>(id_storage, Invalid_id, new_id))
      {
        // Already allocated by someone else.
        free_id(new_id);
        return access_once(id_storage);
      }

    return new_id;
  }

  /**
   * Atomically reset the given `id_storage`.
   *
   * \param id_storage  The ID storage to reset.
   *
   * \return The ID previously stored in `id_storage`.
   */
  Id reset_id_if_valid(Id *id_storage)
  {
    Id id = access_once(id_storage);
    if (id == Invalid_id)
      return Invalid_id;

    if (!cas<Id>(id_storage, id, Invalid_id))
      // Someone else changed the ID.
      return Invalid_id;

    return id;
  }

  /**
   * Atomically reset the given `id_storage` and free the ID.
   *
   * If the given ID storage contains a valid ID, first atomically reset the
   * ID storage to `Invalid_id`, and then free the previously stored ID.
   *
   * \param id_storage  The ID storage to reset.
   */
  void free_id_if_valid(Id *id_storage)
  {
    Id id = reset_id_if_valid(id_storage);
    if (id != Invalid_id)
      free_id(id);
  }

private:
  Id _max_id;
  using Id_map = Bitmap<Max_nr_id>;
  Id_map *_free_ids;
};
