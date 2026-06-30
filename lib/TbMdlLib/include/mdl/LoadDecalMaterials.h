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

#include "Result.h"
#include "gl/TextureResource.h"

#include <filesystem>

namespace tb
{
class Logger;

namespace gl
{
class MaterialCollection;
} // namespace gl

namespace fs
{
class FileSystem;
} // namespace fs

namespace mdl
{

/**
 * Loads loose decal images (PNG/TGA) found recursively under `root` (e.g. "gfx/decals")
 * in the given filesystem into a single MaterialCollection whose `path()` equals `root`.
 *
 * Each material is named `"{" + <stem>` (the leading brace prepended) to match the
 * in-game infodecal convention: the entity `texture` key holds `{name` and the engine
 * strips the brace to resolve `gfx/decals/name.png`. Naming the loaded material `{name`
 * makes the same stored value resolve in TrenchBroom's MaterialManager too, so the
 * decal both appears in the picker and renders in the viewport.
 *
 * Returns an error if the filesystem scan fails; an empty/absent folder yields a
 * collection with no materials (harmless).
 */
Result<gl::MaterialCollection> loadDecalMaterialCollection(
  const fs::FileSystem& fs,
  const std::filesystem::path& root,
  const gl::CreateTextureResource& createResource);

} // namespace mdl
} // namespace tb
