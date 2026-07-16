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

#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "gl/Material.h"
#include "gl/MaterialCollection.h"
#include "gl/MaterialManager.h"
#include "mdl/DecalCollection.h"
#include "mdl/EntityNodeBase.h"
#include "mdl/Map.h"
#include "mdl/Map_Entities.h"
#include "ui/MapDocument.h"
#include "ui/MaterialBrowser.h"
#include "ui/MaterialBrowserView.h"
#include "ui/ViewConstants.h"

#include "kd/vector_utils.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tb::ui
{
namespace
{

// The folder key used both for the dropdown items and the browser's folder filter: the
// material name's parent path ("{PizzaDoggy" for "{PizzaDoggy/foo", "" for a top-level
// "{foo").
std::string decalFolderKey(const std::string& materialName)
{
  return std::filesystem::path{materialName}.parent_path().generic_string();
}

} // namespace

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

  m_folderChoice = new QComboBox{};
  m_folderChoice->setToolTip(tr("Decal folder (subfolders of gfx/decals)"));
  connect(m_folderChoice, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
    applyFolderFilter();
  });

  auto* controlSizer = new QHBoxLayout{};
  controlSizer->setContentsMargins(
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowVMargin,
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowVMargin);
  controlSizer->setSpacing(LayoutConstants::NarrowHMargin);
  controlSizer->addWidget(m_folderChoice, 1);

  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_browser, 1);
  layout->addLayout(controlSizer, 0);
  setLayout(layout);

  // Rebuild the folder list whenever a document is (re)loaded (single-window mode may
  // switch games/maps).
  m_notifierConnection +=
    document().documentWasLoadedNotifier.connect([this]() { reloadFolders(); });

  reloadFolders();
}

void SmartDecalEditor::reloadFolders()
{
  const auto previousData =
    m_folderChoice->count() > 0 ? m_folderChoice->currentData() : QVariant{};

  const auto blocker = QSignalBlocker{m_folderChoice};
  m_folderChoice->clear();

  // "All folders" is always first (index 0) => no folder filter (the flat view).
  m_folderChoice->addItem(tr("All folders"));

  // Gather the distinct subfolders of the loaded gfx/decals collection. Enumeration only
  // reads material names (no texture load), so it is cheap and cannot fail.
  auto folders = std::vector<std::string>{};
  const auto& materialManager = document().map().materialManager();
  for (const auto& collection : materialManager.collections())
  {
    if (collection.path() != mdl::DecalMaterialCollectionPath)
    {
      continue;
    }
    for (const auto& material : collection.materials())
    {
      folders.push_back(decalFolderKey(material.name()));
    }
  }
  folders = kdl::vec_sort_and_remove_duplicates(std::move(folders));

  for (const auto& folder : folders)
  {
    // Label strips the leading brace of the folder key; the raw key is the item data.
    const auto label = folder.empty()
                         ? tr("gfx/decals (top level)")
                         : QString::fromStdString(
                             folder.front() == '{' ? folder.substr(1) : folder);
    m_folderChoice->addItem(label, QString::fromStdString(folder));
  }

  // Restore the previously selected folder if it still exists, else fall back to "All".
  if (previousData.isValid())
  {
    const auto idx = m_folderChoice->findData(previousData);
    m_folderChoice->setCurrentIndex(idx >= 0 ? idx : 0);
  }
  else
  {
    m_folderChoice->setCurrentIndex(0);
  }

  applyFolderFilter();
}

void SmartDecalEditor::applyFolderFilter()
{
  if (m_folderChoice->currentIndex() <= 0)
  {
    m_browser->setFolderFilter(std::nullopt); // "All folders"
  }
  else
  {
    m_browser->setFolderFilter(m_folderChoice->currentData().toString().toStdString());
  }
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
  // Populate the folder list lazily if the decal collection wasn't ready at construction
  // (only the "All folders" entry present so far).
  if (m_folderChoice->count() <= 1)
  {
    reloadFolders();
  }

  const auto value = mdl::selectPropertyValue(propertyKey(), nodes);

  // Open the picker on the folder of the current decal value, if it is listed.
  if (!value.empty())
  {
    const auto folder = decalFolderKey(value);
    const auto idx = m_folderChoice->findData(QString::fromStdString(folder));
    const auto blocker = QSignalBlocker{m_folderChoice};
    m_folderChoice->setCurrentIndex(idx >= 0 ? idx : 0);
    applyFolderFilter();
  }

  // Highlight the current `texture` value in the browser.
  const auto* material = document().map().materialManager().material(value);
  m_browser->setSelectedMaterial(material);
}

} // namespace tb::ui
