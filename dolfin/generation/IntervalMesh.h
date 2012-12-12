// Copyright (C) 2007 Kristian B. Oelgaard
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
// Modified by N. Lopes, 2008.
//
// First added:  2007-11-23
// Last changed: 2012-11-12

#ifndef __INTERVAL_MESH_H
#define __INTERVAL_MESH_H

#include "dolfin/mesh/Mesh.h"

namespace dolfin
{

  /// Interval mesh of the 1D line [a,b].  Given the number of cells
  /// (nx) in the axial direction, the total number of intervals will
  /// be nx and the total number of vertices will be (nx + 1).

  class IntervalMesh : public Mesh
  {
  public:

    /// Constructor
    ///
    /// *Arguments*
    ///     nx (std::size_t)
    ///         The number of cells.
    ///     a (double)
    ///         The minimum point (inclusive).
    ///     b (double)
    ///         The maximum point (inclusive).
    ///
    /// *Example*
    ///     .. code-block:: c++
    ///
    ///         // Create a mesh of 25 cells in the interval [-1,1]
    ///         Interval mesh(25, -1.0, 1.0);
    ///
    IntervalMesh(std::size_t nx, double a, double b);

  };

}

#endif
