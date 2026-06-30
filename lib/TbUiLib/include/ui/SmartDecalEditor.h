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

#include "ui/SmartPropertyEditor.h"

#include <vector>

namespace tb
{
namespace gl
{
class Material;
} // namespace gl

namespace mdl
{
class EntityNodeBase;
} // namespace mdl

namespace ui
{
class AppController;
class MapDocument;
class MaterialBrowser;

// Decal-texture picker for the `texture` key of an infodecal entity. Embeds the real
// MaterialBrowser (thumbnails + sort), scoped to the loose decal PNGs loaded from
// gfx/decals (the dedicated DecalMaterialCollectionPath collection, materials named
// `{<stem>`); clicking a thumbnail sets the key. Companion to the FGD `decal()`
// directive (EntityDecalRenderer then projects it on the surface).
class SmartDecalEditor : public SmartPropertyEditor
{
  Q_OBJECT
private:
  MaterialBrowser* m_browser = nullptr;

public:
  explicit SmartDecalEditor(
    AppController& appController, MapDocument& document, QWidget* parent = nullptr);

private:
  void createGui(AppController& appController);
  void onMaterialSelected(const gl::Material* material);
  void doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes) override;
};

} // namespace ui
} // namespace tb
