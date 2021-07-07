// Copyright 2017-2021 The Verible Authors.
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

#include "verilog/formatting/layout_optimizer.h"
#include "common/formatting/state_node.h"

namespace verilog {
namespace formatter {

// Layout type
enum class LayoutType {
  // Holds UnwrappedLine
  kLayoutLine,

  // Merges sublayouts, horizontally
  kLayoutHorizontalMerge,

  // Merges sublayout, vertically
  kLayoutVerticalMerge,

  // Choice between sublayouts
  kLayoutChoice,

  // Replace with kLayoutChoice and number of
  // kLayoutHorizontalMerge and kLayoutVerticalMerge combinations
  kLayoutWrap,
};

std::ostream& operator<<(std::ostream& stream, const LayoutType& type) {
  switch (type) {
    case LayoutType::kLayoutLine:
      return stream << "[<line>]";
    case LayoutType::kLayoutHorizontalMerge:
      return stream << "[<horizontal>]";
    case LayoutType::kLayoutVerticalMerge:
      return stream << "[<vertical>]";
    case LayoutType::kLayoutChoice:
      return stream << "[<choice>]";
    case LayoutType::kLayoutWrap:
      return stream << "[<wrap>]";
  }
  LOG(FATAL) << "Unknown layout type " << int(type);
  return stream;
}

// Intermediate partition tree layout
class Layout {
 public:
  Layout(LayoutType type) : type_(type), indentation_(0) {}

  Layout(const verible::UnwrappedLine& uwline)
      : type_(LayoutType::kLayoutLine) {
    indentation_ = uwline.IndentationSpaces();
    tokens_ = uwline.TokensRange();
  }

  ~Layout() = default;
  Layout(const Layout&) = default;

  // Deleting standard interfaces
  Layout() = delete;
  Layout(Layout&&) = delete;
  Layout& operator=(const Layout&) = delete;
  Layout& operator=(Layout&&) = delete;

  const LayoutType GetType() const {
    return type_;
  }

  int GetIndentationSpaces() const {
    return indentation_;
  }

  std::string Text() const {
    return absl::StrJoin(tokens_, " ",
                         [=](std::string* out, const verible::PreFormatToken& token) {
                           absl::StrAppend(out, token.Text());
                         });
  }

  int Length() const {
    assert(tokens_.size() > 0);
    int len = 0;
    for (const auto& token : tokens_) {
      len += token.before.spaces_required;
      len += token.Length();
    }
    len -= tokens_[0].before.spaces_required;
    return len;
  }

  bool MustWrap() const {
    assert(tokens_.size() > 0);
    return tokens_[0].before.break_decision == verible::SpacingOptions::MustWrap;
  }

  bool MustAppend() const {
    assert(tokens_.size() > 0);
    return tokens_[0].before.break_decision == verible::SpacingOptions::MustAppend;
  }

  int GetSpacesBefore() const {
    assert(tokens_.size() > 0);
    return tokens_[0].before.spaces_required;
  }

 private:
  const LayoutType type_;

  int indentation_;

  verible::FormatTokenRange tokens_;
};

std::ostream& operator<<(std::ostream& stream, const Layout& layout) {
  const auto type = layout.GetType();
  if (type == LayoutType::kLayoutLine) {
    return stream << "[" << layout.Text() << "]" <<
           ", spacing: " << layout.GetSpacesBefore() <<
           ", length: " << layout.Length() <<
           (layout.MustWrap()?", must-wrap":
           (layout.MustAppend()?", must-append":""));
  }
  return stream << layout.GetType();
}

using LayoutTree = verible::VectorTree<const Layout>;

class Knot {
  friend std::ostream& operator<<(std::ostream&, const Knot&);

 public:
  Knot(int column, int span, int intercept, int gradient,
       std::shared_ptr<const LayoutTree> layout)
      : column_(column), span_(span), intercept_(intercept),
        gradient_(gradient), layout_(layout) {}

  Knot(const Knot&) = default;
  Knot(Knot&&) = default;

  ~Knot() = default;

  // Deleting standard interfaces:
  Knot() = delete;
  Knot& operator=(const Knot&) = delete;
  Knot& operator=(Knot&&) = delete;

  int GetColumn() const { return column_; }
  int GetSpan() const { return span_; }
  int GetIntercept() const { return intercept_; }
  int GetGradient() const { return gradient_; }
  std::shared_ptr<const LayoutTree> GetLayout() const { return layout_; }

 private:
  // Start column
  const int column_;

  // Span of Knot
  const int span_;

  // Constant cost of this knot
  const int intercept_;

  // Cost of over limit characters from this knot
  // cost = intercept_ + (over_limit_characters) * gradient_
  const int gradient_;

  // Layout (subsolution)
  std::shared_ptr<const LayoutTree> layout_;
};

std::ostream& operator<<(std::ostream& stream, const Knot& knot) {
  return stream << "(column: " << knot.column_ <<
                   ", span: " << knot.span_ <<
                   ", intercept: " << knot.intercept_ <<
                   ", gradient: " << knot.gradient_ <<
                   ", layout_tree:\n    " << *knot.layout_ << ")\n";
}

class KnotSet {
  friend std::ostream& operator<<(std::ostream&, const KnotSet&);

 public:
  KnotSet() = default;
  ~KnotSet() = default;

  KnotSet(KnotSet&&) = default;

  // Deleting standard interfaces:
  KnotSet(const KnotSet&) = delete;
  KnotSet& operator=(const KnotSet&) = delete;
  KnotSet& operator=(KnotSet&&) = delete;

  const Knot& operator[](size_t idx) const {
    assert(idx < knots_.size());
    return knots_[idx];
  }

  void AppendKnot(const Knot& knot) {
    knots_.push_back(knot);
  }

  KnotSet PlusConst(int const_val) const {
    KnotSet ret;
    for (const auto& itr : knots_) {
      ret.AppendKnot(Knot{itr.GetColumn(),
                          itr.GetSpan(),
                          itr.GetIntercept() + const_val,
                          itr.GetGradient(),
                          itr.GetLayout()});
    }
    return ret;
  }

 private:
  std::vector</* const */ Knot> knots_;
};

std::ostream& operator<<(std::ostream& stream, const KnotSet& knot_set) {
  stream << "{\n";
  for (const auto& itr : knot_set.knots_) {
    stream << "  " << itr;
  }
  stream << "}\n";
  return stream;
}

class SolutionSet {
 public:
  SolutionSet() = default;
  ~SolutionSet() = default;

  // Deleting standard interfaces:
  SolutionSet(const SolutionSet&) = delete;
  SolutionSet(SolutionSet&&) = delete;
  SolutionSet& operator=(const SolutionSet&) = delete;
  SolutionSet& operator=(SolutionSet&&) = delete;

 private:
  std::vector<std::shared_ptr<const KnotSet*>> solutions_;
};

void OptimizeTokenPartitionTree(verible::TokenPartitionTree* node,
                                const verible::BasicFormatStyle& style) {
  VLOG(4) << "Optimize token partition tree:\n" << *node;

  // orig tree
  {
    const auto orig_tree = *ABSL_DIE_IF_NULL(node);

    std::function<void(const verible::TokenPartitionTree&)> TraverseTree =
        [&TraverseTree,&style](const verible::TokenPartitionTree& n) {
          if (n.Children().size() > 0) {
            VLOG(4) << "policy: " << n.Value().PartitionPolicy();
            for (const auto& child : n.Children()) {
              TraverseTree(child);
            }
          } else {
            KnotSet knot_set;

            auto layout = Layout(n.Value());

            auto layout_tree = std::make_shared<LayoutTree>(layout);

            const auto span = layout.Length();
            if (span < style.column_limit) {
              knot_set.AppendKnot(Knot(0, span, 0, 0, layout_tree));
              knot_set.AppendKnot(Knot(style.column_limit - span,
                                       span, 0, style.over_column_limit_penalty,
                                       layout_tree));
            } else {
              knot_set.AppendKnot(
                  Knot(0, span,
                       (span - style.column_limit) * style.over_column_limit_penalty,
                       style.over_column_limit_penalty, layout_tree));
            }

            VLOG(4) << "knot_set:\n" << knot_set;
          }
        };

    TraverseTree(orig_tree);
  }
}

}  // formatter
}  // verilog
