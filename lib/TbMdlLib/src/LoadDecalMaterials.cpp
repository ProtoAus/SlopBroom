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

#include "mdl/LoadDecalMaterials.h"

#include "fs/FileSystem.h"
#include "fs/PathMatcher.h"
#include "fs/TraversalMode.h"
#include "gl/Material.h"
#include "gl/MaterialCollection.h"
#include "mdl/LoadTexture.h"
#include "mdl/MaterialUtils.h"

#include "kd/path_utils.h"
#include "kd/result.h"

#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{

Result<gl::MaterialCollection> loadDecalMaterialCollection(
  const fs::FileSystem& fs,
  const std::filesystem::path& root,
  const gl::CreateTextureResource& createResource)
{
  const auto extensions = std::vector<std::filesystem::path>{".png", ".tga"};
  return fs.find(
           root,
           fs::TraversalMode::Recursive,
           fs::makeExtensionPathMatcher(extensions))
         | kdl::transform([&](auto paths) {
             const auto prefixLength = kdl::path_length(root);

             auto materials = std::vector<gl::Material>{};
             materials.reserve(paths.size());
             for (const auto& path : paths)
             {
               // The loose PNG filenames carry NO brace; the in-game `texture` value and
               // the TB material lookup both use the brace-prefixed form, so prepend it.
               auto name = "{" + getMaterialNameFromPathSuffix(path, prefixLength);

               // The texture loader is deferred; it captures the (long-lived) game
               // filesystem by reference, matching makeTextureResourceLoader. The name is
               // unused for FreeImage (PNG/TGA) decoding, so the brace is harmless here.
               auto loader = [path, name, &fs]() {
                 return loadTexture(path, name, fs, std::nullopt);
               };

               auto material = gl::Material{name, createResource(std::move(loader))};
               material.setCollectionName(root.generic_string());
               material.setRelativePath(path);
               materials.push_back(std::move(material));
             }

             return gl::MaterialCollection{root, std::move(materials)};
           });
}

} // namespace tb::mdl
