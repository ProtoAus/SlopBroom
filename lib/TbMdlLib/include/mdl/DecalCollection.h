/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <filesystem>

namespace tb::mdl
{

/**
 * Loose decal images (`gfx/decals/<stem>.png`) are loaded as a dedicated material
 * collection whose `path()` equals this value. The single source of truth shared by the
 * loader (`loadDecalMaterialCollection`), the infodecal picker (which scopes its browser
 * to this collection) and the wall/face browser (which excludes it). Keep the strings in
 * lock-step by referencing this constant everywhere rather than re-typing "gfx/decals".
 */
inline const std::filesystem::path DecalMaterialCollectionPath = "gfx/decals";

} // namespace tb::mdl
