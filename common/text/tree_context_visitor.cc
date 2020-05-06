// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/text/tree_context_visitor.h"

#include "common/text/syntax_tree_context.h"

namespace verible {

void TreeContextVisitor::Visit(const SyntaxTreeNode& node) {
  const SyntaxTreeContext::AutoPop p(&current_context_, &node);
  for (const auto& child : node.children()) {
    if (child) child->Accept(this);
  }
}

}  // namespace verible
