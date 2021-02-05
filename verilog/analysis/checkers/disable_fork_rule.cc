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

#include "verilog/analysis/checkers/disable_fork_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/CST/identifier.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register DisableForkNoLabelsRule
VERILOG_REGISTER_LINT_RULE(DisableForkNoLabelsRule);

absl::string_view DisableForkNoLabelsRule::Name() { return "disable-statement"; }
const char DisableForkNoLabelsRule::kTopic[] = "fork-statements";
const char DisableForkNoLabelsRule::kMessage[] =
    "Invalid usage of disable statement. Allowed construction is: disable fork;";

std::string DisableForkNoLabelsRule::GetDescription(DescriptionType description_type) {
  return absl::StrCat("Checks that there are no occurrences of ",
                      Codify("disable some_label", description_type), ". Use ",
                      Codify("disable fork", description_type), " instead. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& DisableMatcher() {
  static const Matcher matcher(
        NodekDisableStatement(DisableStatementHasLabel()));
  return matcher;
}

void DisableForkNoLabelsRule::HandleSymbol(const verible::Symbol& symbol,
                                  const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (DisableMatcher().Matches(symbol, &manager)) {
    const auto disableLabels = FindAllSymbolIdentifierLeafs(symbol);
    if(disableLabels.size() == 0)
    {
      return;
    }
    for(size_t i =0 ; i < context.size()-1; i++)
    {
      auto& n = *(reversed_view(context).begin()+i);
      if(n->Tag().tag == static_cast<int>(NodeEnum::kSeqBlock))
      {
        for(const auto& ch : n->children())
        {
          if(ch.get()->Tag().tag == static_cast<int>(NodeEnum::kBegin))
          {
            const auto beginLabels = FindAllSymbolIdentifierLeafs(*ch);
            if(beginLabels.size() == 0){
              continue;
            }
            const auto beginLabel = SymbolCastToLeaf(*beginLabels[0].match);
            const auto disableLabel = SymbolCastToLeaf(*disableLabels[0].match);
            auto& n = *(reversed_view(context).begin()+i+1);
            auto tag = n->Tag().tag;
            if(tag == static_cast<int>(NodeEnum::kInitialStatement) 
              || tag == static_cast<int>(NodeEnum::kFinalStatement)
              || tag == static_cast<int>(NodeEnum::kAlwaysStatement))
            {
              break;
            }
            if(beginLabel.get().text() == disableLabel.get().text())
            {
              return;
            }
          }
        }
      }
    }
    violations_.insert(LintViolation(symbol, kMessage, context));
  }
}

LintRuleStatus DisableForkNoLabelsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}
}  // namespace analysis
}  // namespace verilog
