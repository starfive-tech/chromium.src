// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_dummy_tree_manager.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

AXDummyTreeManager::AXDummyTreeManager() = default;

AXDummyTreeManager::AXDummyTreeManager(std::unique_ptr<AXTree> tree)
    : AXTreeManager(std::move(tree)) {}

AXDummyTreeManager::AXDummyTreeManager(AXDummyTreeManager&& manager)
    : AXTreeManager(std::move(manager.ax_tree_)) {
  if (ax_tree_) {
    GetMap().RemoveTreeManager(GetTreeID());
    GetMap().AddTreeManager(GetTreeID(), this);
  }
}

AXDummyTreeManager& AXDummyTreeManager::operator=(
    AXDummyTreeManager&& manager) {
  if (this == &manager)
    return *this;
  if (manager.ax_tree_)
    GetMap().RemoveTreeManager(manager.GetTreeID());
  // std::move(nullptr) == nullptr, so no need to check if `manager.tree_` is
  // assigned.
  SetTree(std::move(manager.ax_tree_));
  return *this;
}

AXDummyTreeManager::~AXDummyTreeManager() = default;

void AXDummyTreeManager::DestroyTree() {
  if (!ax_tree_)
    return;

  GetMap().RemoveTreeManager(GetTreeID());
  ax_tree_.reset();
}

AXTree* AXDummyTreeManager::GetTree() const {
  DCHECK(ax_tree_) << "Did you forget to call SetTree?";
  return ax_tree_.get();
}

void AXDummyTreeManager::SetTree(std::unique_ptr<AXTree> tree) {
  if (ax_tree_)
    GetMap().RemoveTreeManager(GetTreeID());

  ax_tree_ = std::move(tree);
  ax_tree_id_ = GetTreeID();
  if (ax_tree_)
    GetMap().AddTreeManager(GetTreeID(), this);
}

AXNode* AXDummyTreeManager::GetNodeFromTree(const AXTreeID& tree_id,
                                            const AXNodeID node_id) const {
  return (ax_tree_ && GetTreeID() == tree_id) ? ax_tree_->GetFromId(node_id)
                                              : nullptr;
}

AXNode* AXDummyTreeManager::GetParentNodeFromParentTreeAsAXNode() const {
  AXTreeID parent_tree_id = GetParentTreeID();
  AXDummyTreeManager* parent_manager =
      static_cast<AXDummyTreeManager*>(AXTreeManager::FromID(parent_tree_id));
  if (!parent_manager)
    return nullptr;

  std::set<AXNodeID> host_node_ids =
      parent_manager->GetTree()->GetNodeIdsForChildTreeId(GetTreeID());

  for (AXNodeID host_node_id : host_node_ids) {
    AXNode* parent_node =
        parent_manager->GetNodeFromTree(parent_tree_id, host_node_id);
    if (parent_node)
      return parent_node;
  }

  return nullptr;
}

}  // namespace ui
