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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tb::mdl
{

/**
 * A parsed Valve Map Format (VMF) block, e.g. `world { ... }`, `solid { ... }`, `side { ... }`.
 * VMF is a tree of named blocks holding `"key" "value"` pairs (keys may repeat — e.g. an
 * object's `editor` block can list `"visgroupid"` several times for multi-membership) and
 * nested child blocks.
 */
struct VmfNode
{
  std::string name;
  std::vector<std::pair<std::string, std::string>> properties; // ordered; keys may repeat
  std::vector<VmfNode> children;

  /** First value for `key`, or nullptr. */
  const std::string* property(std::string_view key) const;
  /** All values for `key` (for repeated keys like "visgroupid"). */
  std::vector<std::string> propertyValues(std::string_view key) const;
  /** All direct children whose name == `childName`. */
  std::vector<const VmfNode*> childrenNamed(std::string_view childName) const;
};

/** Parse VMF text into its top-level blocks. */
Result<std::vector<VmfNode>> parseVmf(std::string_view text);

/**
 * Convert VMF text to TrenchBroom Valve-format `.map` text, so the result can be opened
 * through the normal map-load pipeline. H1 emits the worldspawn brushes; later phases add
 * entities and the VisGroup `_tb_visgroup_*` properties.
 */
Result<std::string> convertVmfToMapText(std::string_view vmfText);

} // namespace tb::mdl
