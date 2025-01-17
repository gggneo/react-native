/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "DOM.h"
#include <react/renderer/components/text/RawTextShadowNode.h>
#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Rect.h>
#include <react/renderer/graphics/Size.h>

namespace {

using namespace facebook::react;

// To prevent ambiguity with built-in MacOS types.
using facebook::react::Point;
using facebook::react::Rect;
using facebook::react::Size;

constexpr uint_fast16_t DOCUMENT_POSITION_DISCONNECTED = 1;
constexpr uint_fast16_t DOCUMENT_POSITION_PRECEDING = 2;
constexpr uint_fast16_t DOCUMENT_POSITION_FOLLOWING = 4;
constexpr uint_fast16_t DOCUMENT_POSITION_CONTAINS = 8;
constexpr uint_fast16_t DOCUMENT_POSITION_CONTAINED_BY = 16;

ShadowNode::Shared getShadowNodeInRevision(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  // If the given shadow node is of the same family as the root shadow node,
  // return the latest root shadow node
  if (ShadowNode::sameFamily(*currentRevision, shadowNode)) {
    return currentRevision;
  }

  auto ancestors = shadowNode.getFamily().getAncestors(*currentRevision);

  if (ancestors.empty()) {
    return nullptr;
  }

  auto pair = ancestors.rbegin();
  return pair->first.get().getChildren().at(pair->second);
}

ShadowNode::Shared getParentShadowNodeInRevision(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  // If the given shadow node is of the same family as the root shadow node,
  // return the latest root shadow node
  if (ShadowNode::sameFamily(*currentRevision, shadowNode)) {
    return currentRevision;
  }

  auto ancestors = shadowNode.getFamily().getAncestors(*currentRevision);

  if (ancestors.empty()) {
    return nullptr;
  }

  if (ancestors.size() == 1) {
    // The parent is the shadow root
    return currentRevision;
  }

  auto parentOfParentPair = ancestors[ancestors.size() - 2];
  return parentOfParentPair.first.get().getChildren().at(
      parentOfParentPair.second);
}

ShadowNode::Shared getPositionedAncestorOfShadowNodeInRevision(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto ancestors = shadowNode.getFamily().getAncestors(*currentRevision);

  if (ancestors.empty()) {
    return nullptr;
  }

  for (auto it = ancestors.rbegin(); it != ancestors.rend(); it++) {
    const auto layoutableAncestorShadowNode =
        dynamic_cast<const LayoutableShadowNode*>(&(it->first.get()));
    if (layoutableAncestorShadowNode == nullptr) {
      return nullptr;
    }
    if (layoutableAncestorShadowNode->getLayoutMetrics().positionType !=
        PositionType::Static) {
      // We have found our nearest positioned ancestor, now to get a shared
      // pointer of it
      it++;
      if (it != ancestors.rend()) {
        return it->first.get().getChildren().at(it->second);
      }
      // else the positioned ancestor is the root which we return outside of the
      // loop
    }
  }

  // If there is no positioned ancestor then we just consider the root
  // to be one
  return currentRevision;
}

void getTextContentInShadowNode(
    const ShadowNode& shadowNode,
    std::string& result) {
  auto rawTextShadowNode = dynamic_cast<const RawTextShadowNode*>(&shadowNode);

  if (rawTextShadowNode != nullptr) {
    result.append(rawTextShadowNode->getConcreteProps().text);
  }

  for (const auto& childNode : shadowNode.getChildren()) {
    getTextContentInShadowNode(*childNode, result);
  }
}

LayoutMetrics getRelativeLayoutMetrics(
    const ShadowNode& ancestorNode,
    const ShadowNode& shadowNode,
    LayoutableShadowNode::LayoutInspectingPolicy policy) {
  auto layoutableAncestorShadowNode =
      dynamic_cast<const LayoutableShadowNode*>(&ancestorNode);

  if (layoutableAncestorShadowNode == nullptr) {
    return EmptyLayoutMetrics;
  }

  return LayoutableShadowNode::computeRelativeLayoutMetrics(
      shadowNode.getFamily(), *layoutableAncestorShadowNode, policy);
}

Rect getScrollableContentBounds(
    Rect contentBounds,
    LayoutMetrics layoutMetrics) {
  auto paddingFrame = layoutMetrics.getPaddingFrame();

  auto paddingBottom =
      layoutMetrics.contentInsets.bottom - layoutMetrics.borderWidth.bottom;
  auto paddingLeft =
      layoutMetrics.contentInsets.left - layoutMetrics.borderWidth.left;
  auto paddingRight =
      layoutMetrics.contentInsets.right - layoutMetrics.borderWidth.right;

  auto minY = paddingFrame.getMinY();
  auto maxY =
      std::max(paddingFrame.getMaxY(), contentBounds.getMaxY() + paddingBottom);

  auto minX = layoutMetrics.layoutDirection == LayoutDirection::RightToLeft
      ? std::min(paddingFrame.getMinX(), contentBounds.getMinX() - paddingLeft)
      : paddingFrame.getMinX();
  auto maxX = layoutMetrics.layoutDirection == LayoutDirection::RightToLeft
      ? paddingFrame.getMaxX()
      : std::max(
            paddingFrame.getMaxX(), contentBounds.getMaxX() + paddingRight);

  return Rect{Point{minX, minY}, Size{maxX - minX, maxY - minY}};
}

} // namespace

namespace facebook::react::dom {

ShadowNode::Shared getParentNode(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  return getParentShadowNodeInRevision(currentRevision, shadowNode);
}

std::optional<std::vector<ShadowNode::Shared>> getChildNodes(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  return shadowNodeInCurrentRevision->getChildren();
}

bool isConnected(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  return shadowNodeInCurrentRevision != nullptr;
}

uint_fast16_t compareDocumentPosition(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode,
    const ShadowNode& otherShadowNode) {
  if (shadowNode.getSurfaceId() != otherShadowNode.getSurfaceId()) {
    return DOCUMENT_POSITION_DISCONNECTED;
  }

  // Quick check for node vs. itself
  if (&shadowNode == &otherShadowNode) {
    return 0;
  }

  auto ancestors = shadowNode.getFamily().getAncestors(*currentRevision);
  if (ancestors.empty()) {
    return DOCUMENT_POSITION_DISCONNECTED;
  }

  auto otherAncestors =
      otherShadowNode.getFamily().getAncestors(*currentRevision);
  if (ancestors.empty()) {
    return DOCUMENT_POSITION_DISCONNECTED;
  }

  // Consume all common ancestors
  size_t i = 0;
  while (i < ancestors.size() && i < otherAncestors.size() &&
         ancestors[i].second == otherAncestors[i].second) {
    i++;
  }

  if (i == ancestors.size()) {
    return (DOCUMENT_POSITION_CONTAINED_BY | DOCUMENT_POSITION_FOLLOWING);
  }

  if (i == otherAncestors.size()) {
    return (DOCUMENT_POSITION_CONTAINS | DOCUMENT_POSITION_PRECEDING);
  }

  if (ancestors[i].second > otherAncestors[i].second) {
    return DOCUMENT_POSITION_PRECEDING;
  }

  return DOCUMENT_POSITION_FOLLOWING;
}

std::string getTextContent(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  std::string result;

  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);

  if (shadowNodeInCurrentRevision != nullptr) {
    getTextContentInShadowNode(*shadowNodeInCurrentRevision, result);
  }

  return result;
}

std::optional<std::tuple<
    /* x: */ double,
    /* y: */ double,
    /* width: */ double,
    /* height: */
    double>>
getBoundingClientRect(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode,
    bool includeTransform) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      shadowNode,
      {.includeTransform = includeTransform, .includeViewportOffset = true});

  if (layoutMetrics == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto frame = layoutMetrics.frame;
  return std::tuple{
      frame.origin.x, frame.origin.y, frame.size.width, frame.size.height};
}

std::optional<std::tuple<
    /* offsetParent: */ ShadowNode::Shared,
    /* top: */ double,
    /* left: */
    double>>
getOffset(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  auto positionedAncestorOfShadowNodeInCurrentRevision =
      getPositionedAncestorOfShadowNodeInRevision(currentRevision, shadowNode);

  // The node is no longer part of an active shadow tree, or it is the
  // root node
  if (shadowNodeInCurrentRevision == nullptr ||
      positionedAncestorOfShadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  // If the node is not displayed (itself or any of its ancestors has
  // "display: none"), this returns an empty layout metrics object.
  auto shadowNodeLayoutMetricsRelativeToRoot = getRelativeLayoutMetrics(
      *currentRevision, shadowNode, {.includeTransform = false});
  if (shadowNodeLayoutMetricsRelativeToRoot == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto positionedAncestorLayoutMetricsRelativeToRoot = getRelativeLayoutMetrics(
      *currentRevision,
      *positionedAncestorOfShadowNodeInCurrentRevision,
      {.includeTransform = false});
  if (positionedAncestorLayoutMetricsRelativeToRoot == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto shadowNodeOriginRelativeToRoot =
      shadowNodeLayoutMetricsRelativeToRoot.frame.origin;
  auto positionedAncestorOriginRelativeToRoot =
      positionedAncestorLayoutMetricsRelativeToRoot.frame.origin;

  // On the Web, offsets are computed from the inner border of the
  // parent.
  auto offsetTop = shadowNodeOriginRelativeToRoot.y -
      positionedAncestorOriginRelativeToRoot.y -
      positionedAncestorLayoutMetricsRelativeToRoot.borderWidth.top;
  auto offsetLeft = shadowNodeOriginRelativeToRoot.x -
      positionedAncestorOriginRelativeToRoot.x -
      positionedAncestorLayoutMetricsRelativeToRoot.borderWidth.left;

  return std::tuple{
      positionedAncestorOfShadowNodeInCurrentRevision, offsetTop, offsetLeft};
}

std::optional<std::tuple</* scrollLeft: */ double, /* scrollTop: */ double>>
getScrollPosition(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  // If the node is not displayed (itself or any of its ancestors has
  // "display: none"), this returns an empty layout metrics object.
  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = true});

  if (layoutMetrics == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto layoutableShadowNode = dynamic_cast<LayoutableShadowNode const*>(
      shadowNodeInCurrentRevision.get());
  // This should never happen
  if (layoutableShadowNode == nullptr) {
    return std::nullopt;
  }

  auto scrollPosition = layoutableShadowNode->getContentOriginOffset();

  return std::tuple{
      scrollPosition.x == 0 ? 0 : -scrollPosition.x,
      scrollPosition.y == 0 ? 0 : -scrollPosition.y};
}

std::optional<std::tuple</* scrollWidth: */ int, /* scrollHeight */ int>>
getScrollSize(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  // If the node is not displayed (itself or any of its ancestors has
  // "display: none"), this returns an empty layout metrics object.
  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = false});

  if (layoutMetrics == EmptyLayoutMetrics ||
      layoutMetrics.displayType == DisplayType::Inline) {
    return std::nullopt;
  }

  auto layoutableShadowNode = dynamic_cast<YogaLayoutableShadowNode const*>(
      shadowNodeInCurrentRevision.get());
  // This should never happen
  if (layoutableShadowNode == nullptr) {
    return std::nullopt;
  }

  Size scrollSize = getScrollableContentBounds(
                        layoutableShadowNode->getContentBounds(), layoutMetrics)
                        .size;

  return std::tuple{
      std::round(scrollSize.width), std::round(scrollSize.height)};
}

std::optional<std::tuple</* width: */ int, /* height: */ int>> getInnerSize(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  // If the node is not displayed (itself or any of its ancestors has
  // "display: none"), this returns an empty layout metrics object.
  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = false});

  if (layoutMetrics == EmptyLayoutMetrics ||
      layoutMetrics.displayType == DisplayType::Inline) {
    return std::nullopt;
  }

  auto paddingFrame = layoutMetrics.getPaddingFrame();

  return std::tuple{
      std::round(paddingFrame.size.width),
      std::round(paddingFrame.size.height)};
}

std::optional<std::tuple<
    /* topWidth: */ int,
    /* rightWidth: */ int,
    /* bottomWidth: */ int,
    /* leftWidth: */
    int>>
getBorderSize(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  // If the node is not displayed (itself or any of its ancestors has
  // "display: none"), this returns an empty layout metrics object.
  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = false});

  if (layoutMetrics == EmptyLayoutMetrics ||
      layoutMetrics.displayType == DisplayType::Inline) {
    return std::nullopt;
  }

  return std::tuple{
      std::round(layoutMetrics.borderWidth.top),
      std::round(layoutMetrics.borderWidth.right),
      std::round(layoutMetrics.borderWidth.bottom),
      std::round(layoutMetrics.borderWidth.left)};
}

std::string getTagName(const ShadowNode& shadowNode) {
  std::string canonicalComponentName = shadowNode.getComponentName();

  // FIXME(T162807327): Remove Android-specific prefixes and unify
  // shadow node implementations
  if (canonicalComponentName == "AndroidTextInput") {
    canonicalComponentName = "TextInput";
  } else if (canonicalComponentName == "AndroidSwitch") {
    canonicalComponentName = "Switch";
  }

  // Prefix with RN:
  canonicalComponentName.insert(0, "RN:");

  return canonicalComponentName;
}

std::optional<std::tuple<
    /* x: */ double,
    /* y: */ double,
    /* width: */ double,
    /* height: */ double,
    /* pageX: */ double,
    /* pageY: */ double>>
measure(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = true, .includeViewportOffset = false});

  if (layoutMetrics == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto layoutableShadowNode = dynamic_cast<const LayoutableShadowNode*>(
      shadowNodeInCurrentRevision.get());
  Point originRelativeToParent = layoutableShadowNode != nullptr
      ? layoutableShadowNode->getLayoutMetrics().frame.origin
      : Point();

  auto frame = layoutMetrics.frame;

  return std::tuple{
      (double)originRelativeToParent.x,
      (double)originRelativeToParent.y,
      (double)frame.size.width,
      (double)frame.size.height,
      (double)frame.origin.x,
      (double)frame.origin.y};
}

std::optional<std::tuple<
    /* x: */ double,
    /* y: */ double,
    /* width: */ double,
    /* height: */ double>>
measureInWindow(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  auto layoutMetrics = getRelativeLayoutMetrics(
      *currentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = true, .includeViewportOffset = true});

  if (layoutMetrics == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto frame = layoutMetrics.frame;
  return std::tuple{
      (double)frame.origin.x,
      (double)frame.origin.y,
      (double)frame.size.width,
      (double)frame.size.height,
  };
}

std::optional<std::tuple<
    /* x: */ double,
    /* y: */ double,
    /* width: */ double,
    /* height: */ double>>
measureLayout(
    const RootShadowNode::Shared& currentRevision,
    const ShadowNode& shadowNode,
    const ShadowNode& relativeToShadowNode) {
  auto shadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, shadowNode);
  if (shadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  auto relativeToShadowNodeInCurrentRevision =
      getShadowNodeInRevision(currentRevision, relativeToShadowNode);
  if (relativeToShadowNodeInCurrentRevision == nullptr) {
    return std::nullopt;
  }

  auto layoutMetrics = getRelativeLayoutMetrics(
      *relativeToShadowNodeInCurrentRevision,
      *shadowNodeInCurrentRevision,
      {.includeTransform = false});

  if (layoutMetrics == EmptyLayoutMetrics) {
    return std::nullopt;
  }

  auto frame = layoutMetrics.frame;

  return std::tuple{
      (double)frame.origin.x,
      (double)frame.origin.y,
      (double)frame.size.width,
      (double)frame.size.height,
  };
}

} // namespace facebook::react::dom
