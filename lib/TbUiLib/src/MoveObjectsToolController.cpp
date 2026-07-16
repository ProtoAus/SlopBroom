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

#include "ui/MoveObjectsToolController.h"

#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityNodeBase.h"
#include "mdl/Grid.h"
#include "mdl/Hit.h"
#include "mdl/HitFilter.h"
#include "mdl/Map.h"
#include "mdl/ModelUtils.h"
#include "mdl/Selection.h"
#include "render/RenderContext.h"
#include "ui/GestureTracker.h"
#include "ui/MoveHandleDragTracker.h"
#include "ui/MoveObjectsTool.h"

#include <cassert>
#include <optional>

namespace tb::ui
{
namespace
{

class MoveObjectsDragDelegate : public MoveHandleDragTrackerDelegate
{
private:
  MoveObjectsTool& m_tool;

public:
  explicit MoveObjectsDragDelegate(MoveObjectsTool& tool)
    : m_tool{tool}
  {
  }

  DragStatus move(
    const InputState& inputState,
    const DragState& dragState,
    const vm::vec3d& proposedHandlePosition) override
  {
    switch (
      m_tool.move(inputState, proposedHandlePosition - dragState.currentHandlePosition))
    {
    case MoveObjectsTool::MoveResult::Continue:
      return DragStatus::Continue;
    case MoveObjectsTool::MoveResult::Deny:
      return DragStatus::Deny;
    case MoveObjectsTool::MoveResult::Cancel:
      return DragStatus::End;
      switchDefault();
    }
  }

  void end(const InputState& inputState, const DragState&) override
  {
    m_tool.endMove(inputState);
  }

  void cancel(const DragState&) override { m_tool.cancelMove(); }

  void setRenderOptions(
    const InputState&, render::RenderContext& renderContext) const override
  {
    renderContext.setForceShowSelectionGuide();
  }

  DragHandleSnapper makeDragHandleSnapper(
    const InputState&, const SnapMode) const override
  {
    const auto& grid = m_tool.grid();
    const auto& sel = m_tool.map().selection();

    // A lone point entity magnetizes its ORIGIN onto the grid. The drag handle is the picked
    // hit point (not the origin), so the standard relative snapper would preserve any sub-grid
    // offset; snap the origin instead. Brush and multi-object moves keep the relative snap.
    if (
      sel.entities.size() == 1 && sel.brushes.empty() && sel.patches.empty()
      && sel.groups.empty() && sel.entities.front()->entity().pointEntity())
    {
      const auto* entityNode = sel.entities.front();
      return [&grid, entityNode](
               const InputState&,
               const DragState& s,
               const vm::vec3d& proposed) -> std::optional<vm::vec3d> {
        // Recover the drag-start origin from the invariant (origin - handle) so this is robust
        // to mid-drag modifier re-calls, then snap origin+delta onto the grid.
        const auto initialOrigin = entityNode->entity().origin()
                                   - (s.currentHandlePosition - s.initialHandlePosition);
        const auto snappedOrigin =
          grid.snap(initialOrigin + (proposed - s.initialHandlePosition));
        return s.initialHandlePosition + (snappedOrigin - initialOrigin);
      };
    }

    return makeRelativeHandleSnapper(grid);
  }
};

} // namespace

MoveObjectsToolController::MoveObjectsToolController(MoveObjectsTool& tool)
  : m_tool{tool}
{
}

MoveObjectsToolController::~MoveObjectsToolController() = default;

Tool& MoveObjectsToolController::tool()
{
  return m_tool;
}

const Tool& MoveObjectsToolController::tool() const
{
  return m_tool;
}

std::unique_ptr<GestureTracker> MoveObjectsToolController::acceptMouseDrag(
  const InputState& inputState)
{
  using namespace mdl::HitFilters;

  if (
    !inputState.modifierKeysPressed(ModifierKeys::None)
    && !inputState.modifierKeysPressed(ModifierKeys::Alt)
    && !inputState.modifierKeysPressed(ModifierKeys::CtrlCmd)
    && !inputState.modifierKeysPressed(ModifierKeys::CtrlCmd | ModifierKeys::Alt))
  {
    return nullptr;
  }

  // The transitivelySelected() lets the hit query match entities/brushes inside a
  // selected group, even though the entities/brushes aren't selected themselves.

  if (const auto& hit =
        inputState.pickResult().first(type(mdl::nodeHitType()) && transitivelySelected());
      hit.isMatch())
  {
    if (m_tool.startMove(inputState))
    {
      return createMoveHandleDragTracker(
        MoveObjectsDragDelegate{m_tool}, inputState, hit.hitPoint(), hit.hitPoint());
    }
  }

  return nullptr;
}

bool MoveObjectsToolController::cancel()
{
  return false;
}

} // namespace tb::ui
