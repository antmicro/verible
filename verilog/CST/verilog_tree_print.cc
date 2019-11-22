// Copyright 2017-2019 The Verible Authors.
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

// Implementation of VerilogPrettyPrinter

#include "verilog/CST/verilog_tree_print.h"

#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"  // for NodeEnumToString
#include "verilog/parser/verilog_parser.h"  // for verilog_symbol_name

namespace verilog {

VerilogPrettyPrinter::VerilogPrettyPrinter(std::ostream* output_stream,
                                           absl::string_view base)
    : verible::PrettyPrinter(
          output_stream,
          verible::TokenInfo::Context(base, [](std::ostream& stream, int e) {
            stream << verilog_symbol_name(e);
          })) {}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeLeaf& leaf) {
  auto_indent() << verible::TokenWithContext{leaf.get(), context_} << std::endl;
  (*cst_current)["token"] = leaf.get().ToString();
}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeNode& node) {
  std::string tag_info = absl::StrCat(
      "(tag: ", NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag)), ") ");

  auto_indent() << "Node " << tag_info << "{" << std::endl;

  nlohmann::json& curr = *cst_current;
  curr["type"] = NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag));
  cst_current = nullptr;

  {
    std::vector<nlohmann::json> json_nodes;

    const verible::ValueSaver<int> value_saver(&indent_, indent_ + 2);
    for (const auto& child : node.children()) {
      nlohmann::json json_subnode;
      cst_current = &json_subnode;
      // TODO(fangism): display nullptrs or child indices to show position.
      if (child) child->Accept(this);
      cst_current = nullptr;
      if (!json_subnode.empty())
        json_nodes.push_back(json_subnode);
    }

    if (json_nodes.size())
      curr["nodes"] = json_nodes;
  }
  auto_indent() << "}" << std::endl;
  cst_current = &curr;
}

void PrettyPrintVerilogTree(const verible::Symbol& root, absl::string_view base,
                            std::ostream* stream) {
  static nlohmann::json cst;
  VerilogPrettyPrinter printer(stream, base);
  printer.cst_current = &cst;
  root.Accept(&printer);
  std::ofstream fileout("verible.json");
  std::stringstream ss;
  ss << cst.dump(2);
  fileout << ss.str();
}

}  // namespace verilog
