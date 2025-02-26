/*
 Copyright (c) 2008-2022, Benoit AUTHEMAN All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the author or Destrat.io nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//-----------------------------------------------------------------------------
// This file is a part of the QuickQanava software library.
//
// \file	qanDraggableCtrl.cpp
// \author	benoit@destrat.io
// \date	2017 03 15
//-----------------------------------------------------------------------------

// QuickQanava headers
#include "./qanDraggableCtrl.h"
#include "./qanNodeItem.h"
#include "./qanGraph.h"

namespace qan { // ::qan

/* Drag'nDrop Management *///--------------------------------------------------
bool    DraggableCtrl::handleDragEnterEvent(QDragEnterEvent* event)
{
    //qWarning() << "DraggableCtrl::handleDragEnterEvent(): this=" << this;
    //qWarning() << "event->source()=" << event->source();
    //qWarning() << "_targetItem->getAcceptDrops()=" << _targetItem->getAcceptDrops();

    if (_targetItem &&
        _targetItem->getAcceptDrops()) {
        if (event->source() == nullptr) {
            event->accept(); // This is propably a drag initated with type=Drag.Internal, for exemple a connector drop node, accept by default...
            return true;
        }
        else { // Get the source item from the quick drag attached object received
            QQuickItem* sourceItem = qobject_cast<QQuickItem*>(event->source());
            if (sourceItem != nullptr) {
                QVariant draggedStyle = sourceItem->property("draggedStyle"); // The source item (usually a style node or edge delegate must expose a draggedStyle property.
                if (draggedStyle.isValid()) {
                    event->accept();
                    return true;
                }
            }
        }
    }
    return false;
}

void	DraggableCtrl::handleDragMoveEvent(QDragMoveEvent* event)
{
    if (_targetItem &&
        _targetItem->getAcceptDrops()) {
        event->acceptProposedAction();
        event->accept();
    }
}

void	DraggableCtrl::handleDragLeaveEvent(QDragLeaveEvent* event)
{
    if (_targetItem &&
        _targetItem->getAcceptDrops())
        event->ignore();
}

void    DraggableCtrl::handleDropEvent(QDropEvent* event)
{
    if (_targetItem &&
        _targetItem->getAcceptDrops() &&
        event->source() != nullptr) { // Get the source item from the quick drag attached object received
        QQuickItem* sourceItem = qobject_cast<QQuickItem*>(event->source());
        if (sourceItem != nullptr) {
            QVariant draggedStyle = sourceItem->property("draggedStyle"); // The source item (usually a style node or edge delegate must expose a draggedStyle property).
            if (draggedStyle.isValid()) {
                auto style = draggedStyle.value<qan::Style*>();
                if (style != nullptr)
                    _targetItem->setItemStyle(style);
            }
        }
    }
}

void    DraggableCtrl::handleMouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
}

bool    DraggableCtrl::handleMouseMoveEvent(QMouseEvent* event)
{
    // PRECONDITIONS:
        // graph must be non nullptr (configured).
        // _targetItem can't be nullptr
    const auto graph = getGraph();
    if (graph == nullptr)
        return false;
    if (!_targetItem)
        return false;

    // Early exits
    if (event->buttons().testFlag(Qt::NoButton))
        return false;
    if (!_targetItem->getDraggable())
        return false;
    if (_targetItem->getCollapsed())
        return false;

    if (_targetItem->getNode() != nullptr &&
            (_targetItem->getNode()->getLocked() ||
             _targetItem->getNode()->getIsProtected()))
        return false;

    const auto rootItem = getGraph()->getContainerItem();
    if (rootItem != nullptr &&      // Root item exist, left button is pressed and the target item
        event->buttons().testFlag(Qt::LeftButton)) {    // is draggable and not collapsed
        const auto sceneDragPos = rootItem->mapFromGlobal(event->globalPos());
        if (!_targetItem->getDragged()) {
            // Project in scene rect (for example is a node is part of a group)
            beginDragMove(sceneDragPos, _targetItem->getSelected());
            return true;
        } else {
            dragMove(sceneDragPos, _targetItem->getSelected());
            return true;
        }
    }
    return false;
}

void    DraggableCtrl::handleMousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
}

void    DraggableCtrl::handleMouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    if (_targetItem &&
        _targetItem->getDragged())
        endDragMove();
}

void    DraggableCtrl::beginDragMove(const QPointF& sceneDragPos, bool dragSelection)
{
    if (_targetItem == nullptr)
        return;
    const auto graph = getGraph();
    if (graph != nullptr &&
        _target != nullptr)
        emit graph->nodeAboutToBeMoved(_target);
    _targetItem->setDragged(true);
    _initialDragPos = sceneDragPos;
    const auto rootItem = getGraph()->getContainerItem();
    if (rootItem != nullptr)   // Project in scene rect (for example is a node is part of a group)
        _initialTargetPos = rootItem->mapFromItem(_targetItem, QPointF{0,0});

    // If there is a selection, keep start position for all selected nodes.
    if (dragSelection) {
        const auto graph = getGraph();
        if (graph != nullptr &&
            graph->hasMultipleSelection()) {

            auto beginDragMoveSelected = [this, &sceneDragPos] (auto primitive) {    // Call beginDragMove() on a given node or group
                if (primitive != nullptr &&
                    primitive->getItem() != nullptr &&
                    static_cast<QQuickItem*>(primitive->getItem()) != static_cast<QQuickItem*>(this->_targetItem.data()) &&
                    primitive->get_group() == nullptr)      // Do not drag nodes that are inside a group
                    primitive->getItem()->draggableCtrl().beginDragMove(sceneDragPos, false);
            };

            // Call beginDragMove on all selected nodes and groups.
            std::for_each(graph->getSelectedNodes().begin(), graph->getSelectedNodes().end(), beginDragMoveSelected);
            std::for_each(graph->getSelectedGroups().begin(), graph->getSelectedGroups().end(), beginDragMoveSelected);
        }
    }
}

void    DraggableCtrl::dragMove(const QPointF& sceneDragPos, bool dragSelection,
                                bool disableSnapToGrid, bool disableOrientation)
{
    // PRECONDITIONS:
        // _graph must be configured (non nullptr)
        // _graph must have a container item for coordinate mapping
        // _target and _targetItem must be configured (true)
    const auto graph = getGraph();
    if (graph == nullptr)
        return;
    const auto graphContainerItem = graph->getContainerItem();
    if (graphContainerItem == nullptr)
        return;
    if (!_target ||
        !_targetItem)
        return;

    // Algorithm:
        // 1. For grouped node (node insed a group), check if the drag move occurs inside the group.
            // 1.1 For inside
            // 2.1 For outside groups
        // 2. For ungrouped nodes, perform the drag using scene coords.
        // 3. If the node is ungroupped and the drag is not an inside group dragging, propose
        //    the node for grouping (ie just hilight the potential target group item).

    const auto delta = (sceneDragPos - _initialDragPos);
    const auto targetUnsnapPos = _initialTargetPos + delta;

    const auto targetGroup = _target->get_group();
    auto movedInsideGroup = false;
    if (targetGroup &&
        targetGroup->getItem() != nullptr) {
        const QRectF targetRect{
            targetUnsnapPos,
            QSizeF{_targetItem->width(),
                   _targetItem->height()}
        };
        const QRectF groupRect{
            QPointF{0., 0.},
            QSizeF{targetGroup->getItem()->width(),
                   targetGroup->getItem()->height()}
        };

        movedInsideGroup = groupRect.contains(targetRect);
        if (!movedInsideGroup)
            graph->ungroupNode(_target, _target->get_group());
    }

    // Drag algorithm:
    // 2.1. Convert the mouse drag position to "target item" space unspaned pos
    // 2.2. If target position is "centered" on grid
    //    or mouse delta > grid
    //   2.2.1 Compute snapped position, apply it
    const auto targetDragOrientation = _targetItem->getDragOrientation();
    const auto dragHorizontally = disableOrientation ||
                                  ((targetDragOrientation == qan::NodeItem::DragOrientation::DragAll) ||
                                   (targetDragOrientation == qan::NodeItem::DragOrientation::DragHorizontal));
    const auto dragVertically = disableOrientation ||
                                ((targetDragOrientation == qan::NodeItem::DragOrientation::DragAll) ||
                                 (targetDragOrientation == qan::NodeItem::DragOrientation::DragVertical));
    if (!disableSnapToGrid &&
        getGraph()->getSnapToGrid()) {
        const auto& gridSize = getGraph()->getSnapToGridSize();
        bool applyX = dragHorizontally &&
                      std::fabs(delta.x()) > (gridSize.width() / 2.001);
        bool applyY = dragVertically &&
                      std::fabs(delta.y()) > (gridSize.height() / 2.001);
        if (!applyX && dragHorizontally) {
            const auto posModGridX = fmod(targetUnsnapPos.x(), gridSize.width());
            applyX = qFuzzyIsNull(posModGridX);
        }
        if (!applyY && dragVertically) {
            const auto posModGridY = fmod(targetUnsnapPos.y(), gridSize.height());
            applyY = qFuzzyIsNull(posModGridY);
        }
        if (applyX || applyY) {
            const auto targetSnapPosX = dragHorizontally ? gridSize.width() * std::round(targetUnsnapPos.x() / gridSize.width()) :
                                                           _initialTargetPos.x();
            const auto targetSnapPosY = dragVertically ? gridSize.height() * std::round(targetUnsnapPos.y() / gridSize.height()) :
                                                         _initialTargetPos.y();
            _targetItem->setPosition(QPointF{targetSnapPosX,
                                             targetSnapPosY});
        }
    } else { // Do not snap to grid
        _targetItem->setPosition(QPointF{dragHorizontally ? targetUnsnapPos.x() : _initialTargetPos.x(),
                                         dragVertically   ? targetUnsnapPos.y() : _initialTargetPos.y()});
    }

    if (dragSelection) {
        auto dragMoveSelected = [this, &sceneDragPos] (auto primitive) { // Call dragMove() on a given node or group
            const auto primitiveIsNotSelf = static_cast<QQuickItem*>(primitive->getItem()) !=
                                            static_cast<QQuickItem*>(this->_targetItem.data());
            if (primitive != nullptr &&
                primitive->getItem() != nullptr &&
                primitiveIsNotSelf)       // Note: Contrary to beginDragMove(), drag nodes that are inside a group
                primitive->getItem()->draggableCtrl().dragMove(sceneDragPos, false);
        };

        std::for_each(graph->getSelectedNodes().begin(), graph->getSelectedNodes().end(), dragMoveSelected);
        std::for_each(graph->getSelectedGroups().begin(), graph->getSelectedGroups().end(), dragMoveSelected);
    }

    // Eventually, propose a node group drop after move
    if (!movedInsideGroup &&
        _targetItem->getDroppable()) {
        qan::Group* group = graph->groupAt(_targetItem->mapToItem(graphContainerItem, QPointF{0., 0.}),
                                           {_targetItem->width(), _targetItem->height()},
                                           _targetItem /* except _targetItem */);
        if (group != nullptr &&
            !group->getLocked() &&
            group->getItem() != nullptr &&
            static_cast<QQuickItem*>(group->getItem()) != static_cast<QQuickItem*>(_targetItem.data()))  { // Do not drop a group in itself
            group->itemProposeNodeDrop();

            if (_lastProposedGroup &&              // When a node is already beeing proposed in a group (ie _lastProposedGroup is non nullptr), it
                _lastProposedGroup->getItem() != nullptr &&     // might also end up beeing dragged over a sub group of _lastProposedGroup, so
                group != _lastProposedGroup)                    // notify endProposeNodeDrop() for last proposed group
                _lastProposedGroup->itemEndProposeNodeDrop();
            _lastProposedGroup = group;
        } else if (group == nullptr &&
                   _lastProposedGroup != nullptr &&
                   _lastProposedGroup->getItem() != nullptr) {
            _lastProposedGroup->itemEndProposeNodeDrop();
            _lastProposedGroup = nullptr;
        }
    }
}

void    DraggableCtrl::endDragMove(bool dragSelection)
{
    _initialDragPos = QPointF{0., 0.};    // Invalid all cached coordinates when drag ends
    _initialTargetPos = QPointF{0., 0.};
    _lastProposedGroup = nullptr;

    // PRECONDITIONS:
        // _target and _targetItem must be configured (true)
        // _graph must be configured (non nullptr)
        // _graph must have a container item for coordinate mapping
    if (!_target ||
        !_targetItem)
        return;
    const auto graph = getGraph();
    if (graph == nullptr)
        return;
    const auto graphContainerItem = graph->getContainerItem();
    if (graphContainerItem == nullptr)
        return;

    bool nodeGrouped = false;
    if (_targetItem->getDroppable()) {
        const auto targetContainerPos = _targetItem->mapToItem(graphContainerItem, QPointF{0., 0.});
        qan::Group* group = graph->groupAt(targetContainerPos, { _targetItem->width(), _targetItem->height() }, _targetItem);
        if (group != nullptr &&
            static_cast<QQuickItem*>(group->getItem()) != static_cast<QQuickItem*>(_targetItem.data())) { // Do not drop a group in itself
            if (group->getGroupItem() != nullptr &&             // Do not allow grouping a node in a collapsed
                !group->getGroupItem()->getCollapsed() &&       // or locked group item
                !group->getLocked()) {
                graph->groupNode(group, _target.data());
                nodeGrouped = true;
            }
        }
    }
    if (!nodeGrouped)  // Do not emit nodeMoved() if it has been grouped
        emit graph->nodeMoved(_target);

    _targetItem->setDragged(false);

    if (dragSelection &&               // If there is a selection, end drag for the whole selection
        graph->hasMultipleSelection()) {
        auto enDragMoveSelected = [this] (auto primitive) { // Call dragMove() on a given node or group
            if ( primitive != nullptr &&
                 primitive->getItem() != nullptr &&
                 static_cast<QQuickItem*>(primitive->getItem()) != static_cast<QQuickItem*>(this->_targetItem.data()) &&
                 primitive->get_group() == nullptr)        // Do not drag nodes that are inside a group
                primitive->getItem()->draggableCtrl().endDragMove(false);
        };
        std::for_each(graph->getSelectedNodes().begin(), graph->getSelectedNodes().end(), enDragMoveSelected);
        std::for_each(graph->getSelectedGroups().begin(), graph->getSelectedGroups().end(), enDragMoveSelected);
    }
}
//-----------------------------------------------------------------------------

} // ::qan
