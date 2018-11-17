
#include "GenericDofMap.h"
#include <dolfin/common/IndexMap.h>
#include <dolfin/mesh/Cell.h>
#include <dolfin/mesh/MeshIterator.h>
#include <vector>

using namespace dolfin;
using namespace dolfin::fem;

//-----------------------------------------------------------------------------
Eigen::Array<std::size_t, Eigen::Dynamic, 1>
GenericDofMap::tabulate_local_to_global_dofs() const
{
  // FIXME: use common::IndexMap::local_to_global_index?

  const auto idxmap = index_map();
  assert(idxmap);
  const std::size_t bs = idxmap->block_size();
  const auto& local_to_global_unowned = idxmap->ghosts();
  const std::size_t local_ownership_size = bs * idxmap->size_local();

  Eigen::Array<std::size_t, Eigen::Dynamic, 1> local_to_global_map(
      bs * (idxmap->size_local() + idxmap->num_ghosts()));

  const std::size_t global_offset = bs * idxmap->local_range()[0];
  for (std::size_t i = 0; i < local_ownership_size; ++i)
    local_to_global_map[i] = i + global_offset;

  for (Eigen::Index node = 0; node < local_to_global_unowned.size(); ++node)
  {
    for (std::size_t component = 0; component < bs; ++component)
    {
      local_to_global_map[bs * node + component + local_ownership_size]
          = bs * local_to_global_unowned[node] + component;
    }
  }

  return local_to_global_map;
}
//-----------------------------------------------------------------------------
Eigen::Array<PetscInt, Eigen::Dynamic, 1>
GenericDofMap::dofs(const mesh::Mesh& mesh, std::size_t dim) const
{
  // Check number of dofs per entity (on each cell)
  const std::size_t num_dofs_per_entity = num_entity_dofs(dim);

  // Return empty vector if not dofs on requested entity
  if (num_dofs_per_entity == 0)
    return Eigen::Array<PetscInt, Eigen::Dynamic, 1>();

  // Vector to hold list of dofs
  Eigen::Array<PetscInt, Eigen::Dynamic, 1> dof_list(mesh.num_entities(dim)
                                                     * num_dofs_per_entity);

  // Build local dofs for each entity of dimension dim
  const mesh::CellType& cell_type = mesh.type();
  std::vector<Eigen::Array<int, Eigen::Dynamic, 1>> entity_dofs_local;
  for (std::size_t i = 0; i < cell_type.num_entities(dim); ++i)
    entity_dofs_local.push_back(tabulate_entity_dofs(dim, i));

  // Iterate over cells
  for (auto& c : mesh::MeshRange<mesh::Cell>(mesh))
  {
    // Get local-to-global dofmap for cell
    const auto cell_dof_list = cell_dofs(c.index());

    // Loop over all entities of dimension dim
    unsigned int local_index = 0;
    for (auto& e : mesh::EntityRange<mesh::MeshEntity>(c, dim))
    {
      // Get dof index and add to list
      for (Eigen::Index i = 0; i < entity_dofs_local[local_index].size(); ++i)
      {
        const std::size_t entity_dof_local = entity_dofs_local[local_index][i];
        const PetscInt dof_index = cell_dof_list[entity_dof_local];
        assert((Eigen::Index)(e.index() * num_dofs_per_entity + i)
               < dof_list.size());
        dof_list[e.index() * num_dofs_per_entity + i] = dof_index;
      }
      ++local_index;
    }
  }

  return dof_list;
}
//-----------------------------------------------------------------------------
Eigen::Array<PetscInt, Eigen::Dynamic, 1>
GenericDofMap::entity_dofs(const mesh::Mesh& mesh, std::size_t entity_dim,
                           const std::vector<std::size_t>& entity_indices) const
{
  // Get some dimensions
  const std::size_t top_dim = mesh.topology().dim();
  const std::size_t dofs_per_entity = num_entity_dofs(entity_dim);

  // Initialize entity to cell connections
  mesh.init(entity_dim, top_dim);

  // Allocate the the array to return
  const std::size_t num_marked_entities = entity_indices.size();
  Eigen::Array<PetscInt, Eigen::Dynamic, 1> entity_to_dofs(num_marked_entities
                                                           * dofs_per_entity);

  // Build local dofs for each entity of dimension dim
  const mesh::CellType& cell_type = mesh.type();
  std::vector<Eigen::Array<int, Eigen::Dynamic, 1>> local_to_local_map;
  for (std::size_t i = 0; i < cell_type.num_entities(entity_dim); ++i)
    local_to_local_map.push_back(tabulate_entity_dofs(entity_dim, i));

  // Iterate over entities
  std::size_t local_entity_ind = 0;
  for (std::size_t i = 0; i < num_marked_entities; ++i)
  {
    mesh::MeshEntity entity(mesh, entity_dim, entity_indices[i]);

    // Get the first cell connected to the entity
    const mesh::Cell cell(mesh, entity.entities(top_dim)[0]);

    // Find local entity number
    for (std::size_t local_i = 0; local_i < cell.num_entities(entity_dim);
         ++local_i)
    {
      if (cell.entities(entity_dim)[local_i] == entity.index())
      {
        local_entity_ind = local_i;
        break;
      }
    }

    // Get all cell dofs
    const auto cell_dof_list = cell_dofs(cell.index());

    // Fill local dofs for the entity
    for (std::size_t local_dof = 0; local_dof < dofs_per_entity; ++local_dof)
    {
      // Map dofs
      const PetscInt global_dof
          = cell_dof_list[local_to_local_map[local_entity_ind][local_dof]];
      entity_to_dofs[dofs_per_entity * i + local_dof] = global_dof;
    }
  }
  return entity_to_dofs;
}
//-----------------------------------------------------------------------------
Eigen::Array<PetscInt, Eigen::Dynamic, 1>
GenericDofMap::entity_dofs(const mesh::Mesh& mesh, std::size_t entity_dim) const
{
  // Get some dimensions
  const std::size_t top_dim = mesh.topology().dim();
  const std::size_t dofs_per_entity = num_entity_dofs(entity_dim);
  const std::size_t num_mesh_entities = mesh.num_entities(entity_dim);

  // Initialize entity to cell connections
  mesh.init(entity_dim, top_dim);

  // Allocate the the array to return
  Eigen::Array<PetscInt, Eigen::Dynamic, 1> entity_to_dofs(num_mesh_entities
                                                           * dofs_per_entity);

  // Build local dofs for each entity of dimension dim
  const mesh::CellType& cell_type = mesh.type();
  std::vector<Eigen::Array<int, Eigen::Dynamic, 1>> local_to_local_map;
  for (std::size_t i = 0; i < cell_type.num_entities(entity_dim); ++i)
    local_to_local_map.push_back(tabulate_entity_dofs(entity_dim, i));

  // Iterate over entities
  std::size_t local_entity_ind = 0;
  for (auto& entity : mesh::MeshRange<mesh::MeshEntity>(mesh, entity_dim))
  {
    // Get the first cell connected to the entity
    const mesh::Cell cell(mesh, entity.entities(top_dim)[0]);

    // Find local entity number
    for (std::size_t local_i = 0; local_i < cell.num_entities(entity_dim);
         ++local_i)
    {
      if (cell.entities(entity_dim)[local_i] == entity.index())
      {
        local_entity_ind = local_i;
        break;
      }
    }

    // Get all cell dofs
    const auto cell_dof_list = cell_dofs(cell.index());

    // Fill local dofs for the entity
    for (std::size_t local_dof = 0; local_dof < dofs_per_entity; ++local_dof)
    {
      // Map dofs
      const PetscInt global_dof
          = cell_dof_list[local_to_local_map[local_entity_ind][local_dof]];
      entity_to_dofs[dofs_per_entity * entity.index() + local_dof] = global_dof;
    }
  }
  return entity_to_dofs;
}
//-----------------------------------------------------------------------------
void GenericDofMap::ufc_tabulate_dofs(
    int64_t* dofs,
    const std::vector<std::vector<std::vector<int>>>& entity_dofs,
    const int64_t* num_global_entities, const int64_t** entity_indices)
{
  // Loop over cell entity types (vertex, edge, etc)
  std::size_t offset = 0;
  for (std::size_t d = 0; d < entity_dofs.size(); ++d)
  {
    // Loop over each entity of dimension d
    for (std::size_t i = 0; i < entity_dofs[d].size(); ++i)
    {
      const int num_entity_dofs = entity_dofs[d][i].size();

      // Loop over dofs belong to entity e of dimension d (d, e)
      for (std::size_t dof = 0; dof < entity_dofs[d][i].size(); ++dof)
      {
        // d: topological dimension
        // i: local entity index
        // dof: local index of dof at (d, i)
        dofs[entity_dofs[d][i][dof]]
            = offset + num_entity_dofs * entity_indices[d][i] + dof;
      }
    }
    offset += entity_dofs[d][0].size() * num_global_entities[d];
  }
}
//-----------------------------------------------------------------------------
void GenericDofMap::permutation(
    std::vector<int>& perm, mesh::CellType::Type cell_type,
    const std::vector<std::vector<std::vector<int>>>& entity_dofs,
    const int64_t* vertex_indices)
{
  // Reset to identity
  for (unsigned int i = 0; i < perm.size(); ++i)
    perm[i] = i;

  if (cell_type == mesh::CellType::Type::tetrahedron)
  {
    // Get ordering of edges
    bool edge_ordering[6];
    edge_ordering[0] = vertex_indices[2] > vertex_indices[3];
    edge_ordering[1] = vertex_indices[1] > vertex_indices[3];
    edge_ordering[2] = vertex_indices[1] > vertex_indices[2];
    edge_ordering[3] = vertex_indices[0] > vertex_indices[3];
    edge_ordering[4] = vertex_indices[0] > vertex_indices[2];
    edge_ordering[5] = vertex_indices[0] > vertex_indices[1];

    for (unsigned int j = 0; j < 6; ++j)
    {
      if (edge_ordering[j])
      {
        // Reverse dofs along this edge
        const std::vector<int>& edge_dofs = entity_dofs[1][j];
        unsigned int n = edge_dofs.size();
        for (unsigned int i = 0; i < n; ++i)
          perm[edge_dofs[i]] = edge_dofs[n - i - 1];
      }
    }

    // Edges on each facet
    static unsigned int facet_edges[4][3]
        = {{0, 1, 2}, {0, 3, 4}, {1, 3, 5}, {2, 4, 5}};

    // Generate lattice for Lagrange element
    // n = polynomial degree
    // FIXME - how to get n here?
    unsigned int n = 1;
    while (n * (n - 1) / 2 < entity_dofs[2][0].size())
      ++n;
    ++n;

    std::cout << "Guessing P" << n << "\n";

    unsigned int c = 0;
    std::vector<std::vector<unsigned int>> facet_dof_coords(n - 2);
    for (unsigned int j = 0; j < n - 2; ++j)
    {
      for (unsigned int i = 0; i < n - 2 - j; ++i)
      {
        facet_dof_coords[j].push_back(c);
        ++c;
      }
    }

    // Facet ordering
    for (unsigned int m = 0; m < 4; ++m)
    {
      const std::vector<int>& facet_dofs = entity_dofs[2][m];

      if (facet_dofs.size() > 1)
      {
        const unsigned int* fe = facet_edges[m];
        int facet_ordering
            = edge_ordering[fe[0]]
              + 2 * (edge_ordering[fe[1]] + edge_ordering[fe[2]]);

        std::cout << "Ordering = " << facet_ordering << "\n";

        // Do stuff based on value of facet_ordering (0-5)
        switch (facet_ordering)
        {
        case 0:
          break;
        case 1:
          c = 0;
          for (unsigned int j = 0; j < n - 2; ++j)
          {
            for (unsigned int i = 0; i < n - 2 - j; ++i)
            {
              perm[facet_dofs[c]] = facet_dofs[facet_dof_coords[i][j]];
              ++c;
            }
          }
          break;
        case 2:
          c = 0;
          for (unsigned int j = 0; j < n - 2; ++j)
          {
            for (unsigned int i = 0; i < n - 2 - j; ++i)
            {
              unsigned int k = n - i - j - 3;
              perm[facet_dofs[c]] = facet_dofs[facet_dof_coords[j][k]];
              ++c;
            }
          }
          break;
        case 3:
          c = 0;
          for (unsigned int j = 0; j < n - 2; ++j)
          {
            for (unsigned int i = 0; i < n - 2 - j; ++i)
            {
              unsigned int k = n - i - j - 3;
              perm[facet_dofs[c]] = facet_dofs[facet_dof_coords[i][k]];
              ++c;
            }
          }
          break;
        case 4:
          c = 0;
          for (unsigned int j = 0; j < n - 2; ++j)
          {
            for (unsigned int i = 0; i < n - 2 - j; ++i)
            {
              unsigned int k = n - i - j - 3;
              perm[facet_dofs[c]] = facet_dofs[facet_dof_coords[k][j]];
              ++c;
            }
          }
          break;
        case 5:
          c = 0;
          for (unsigned int j = 0; j < n - 2; ++j)
          {
            for (unsigned int i = 0; i < n - 2 - j; ++i)
            {
              unsigned int k = n - i - j - 3;
              perm[facet_dofs[c]] = facet_dofs[facet_dof_coords[k][i]];
              ++c;
            }
          }
          break;
        }
      }
    }
  }
  else if (cell_type == mesh::CellType::Type::triangle)
  {
    // Get ordering of edges
    bool edge_ordering[3];
    edge_ordering[0] = vertex_indices[1] > vertex_indices[2];
    edge_ordering[1] = vertex_indices[0] > vertex_indices[2];
    edge_ordering[2] = vertex_indices[0] > vertex_indices[1];

    for (unsigned int j = 0; j < 3; ++j)
    {
      if (edge_ordering[j])
      {
        // Reverse dofs along this edge
        const std::vector<int>& edge_dofs = entity_dofs[1][j];
        const unsigned int n = edge_dofs.size();
        for (unsigned int i = 0; i < n; ++i)
          perm[edge_dofs[i]] = edge_dofs[n - i - 1];
      }
    }
  }

  // Debug printout
  for (auto& q : perm)
    std::cout << q << " ";
  std::cout << "\n";
}
//-----------------------------------------------------------------------------
