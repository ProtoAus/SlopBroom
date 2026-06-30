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

#include "ui/SmartDecalEditor.h"

#include <QVBoxLayout>

#include "gl/Material.h"
#include "gl/MaterialManager.h"
#include "mdl/DecalCollection.h"
#include "mdl/EntityNodeBase.h"
#include "mdl/Map.h"
#include "mdl/Map_Entities.h"
#include "ui/MapDocument.h"
#include "ui/MaterialBrowser.h"
#include "ui/MaterialBrowserView.h"

namespace tb::ui
{

SmartDecalEditor::SmartDecalEditor(
  AppController& appController, MapDocument& document, QWidget* parent)
  : SmartPropertyEditor{document, parent}
{
  createGui(appController);
}

void SmartDecalEditor::createGui(AppController& appController)
{
  m_browser = new MaterialBrowser{appController, document()};
  m_browser->setSortOrder(MaterialSortOrder::Name); // alphabetical
  m_browser->setHideUnused(false);
  // Show only the loose decal PNGs loaded from gfx/decals (named "{<stem>"), not the
  // wad's textures. See loadDecalMaterialCollection / DecalMaterialCollectionPath.
  m_browser->setScopeCollection(mdl::DecalMaterialCollectionPath);
  connect(
    m_browser,
    &MaterialBrowser::materialSelected,
    this,
    &SmartDecalEditor::onMaterialSelected);

  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_browser, 1);
  setLayout(layout);
}

void SmartDecalEditor::onMaterialSelected(const gl::Material* material)
{
  if (!material)
  {
    return;
  }
  // The browser is scoped to the gfx/decals collection, whose materials are all named
  // "{<stem>"; the brace is what the in-game infodecal expects in the `texture` key
  // (the engine strips it to resolve gfx/decals/<stem>.png). Guard anyway.
  const auto& name = material->name();
  if (name.empty() || name[0] != '{')
  {
    return;
  }
  setEntityProperty(document().map(), propertyKey(), name);
}

void SmartDecalEditor::doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes)
{
  // Highlight the current `texture` value in the browser.
  const auto value = mdl::selectPropertyValue(propertyKey(), nodes);
  const auto* material = document().map().materialManager().material(value);
  m_browser->setSelectedMaterial(material);
}

} // namespace tb::ui
