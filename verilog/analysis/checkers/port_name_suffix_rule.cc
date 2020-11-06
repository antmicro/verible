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

#include "verilog/analysis/checkers/port_name_suffix_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/port.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register PortNameSuffixRule.
VERILOG_REGISTER_LINT_RULE(PortNameSuffixRule);

absl::string_view PortNameSuffixRule::Name() {
  return "port-name-suffix";
}
const char PortNameSuffixRule::kTopic[] = "ports";
const char PortNameSuffixRule::kMessageIn[] =
    "input port names must end with _i";
const char PortNameSuffixRule::kMessageOut[] =
    "output port names must end with _o";
const char PortNameSuffixRule::kMessageInOut[] =
    "inout port names must end with _io";

std::string PortNameSuffixRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Check that port names end with _i for inputs, _o for outputs and _io for inouts. "
      "See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& PortMatcher() {
  static const Matcher matcher(NodekPortDeclaration());
  return matcher;
}

void PortNameSuffixRule::HandleSymbol(const verible::Symbol& symbol,
                                       const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto* identifier_leaf =
        GetIdentifierFromModulePortDeclaration(symbol);
    const auto* direction_leaf =
        GetDirectionFromModulePortDeclaration(symbol);
    const auto direction = ABSL_DIE_IF_NULL(direction_leaf)->get().text();
    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();

    if (direction == "input") {
      if (!absl::EndsWith(name, "_i"))
        violations_.insert(
            LintViolation(identifier_leaf->get(), kMessageIn, context));
    } else if (direction == "output") {
      if (!absl::EndsWith(name, "_o"))
        violations_.insert(
            LintViolation(identifier_leaf->get(), kMessageOut, context));
    } else if (direction == "inout") {
      if (!absl::EndsWith(name, "_io"))
        violations_.insert(
            LintViolation(identifier_leaf->get(), kMessageInOut, context));
    }
  }
}

LintRuleStatus PortNameSuffixRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
