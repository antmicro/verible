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

// verilog_syntax is a simple command-line utility to check Verilog syntax
// for the given file.
//
// Example usage:
// verilog_syntax --verilog_trace_parser files...

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"  // for MakeArraySlice
#include "common/strings/compare.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/parser_verifier.h"
#include "common/text/text_structure.h"
#include "common/text/tree_context_visitor.h"
#include "common/text/token_info.h"
#include "common/util/bijective_map.h"
#include "common/util/enum_flags.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "common/util/value_saver.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_excerpt_parse.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token.h"
#include "verilog/CST/verilog_nonterminals.h"

// Controls parser selection behavior
enum class LanguageMode {
  // May try multiple language options starting with SV, stops on first success.
  kAutoDetect,
  // Strict SystemVerilog 2017, no automatic trying of alternative parsing modes
  kSystemVerilog,
  // Verilog library map sub-language only.  LRM Chapter 33.
  kVerilogLibraryMap,
};

static const verible::EnumNameMap<LanguageMode> kLanguageModeStringMap{{
    {"auto", LanguageMode::kAutoDetect},
    {"sv", LanguageMode::kSystemVerilog},
    {"lib", LanguageMode::kVerilogLibraryMap},
}};

static std::ostream& operator<<(std::ostream& stream, LanguageMode mode) {
  return kLanguageModeStringMap.Unparse(mode, stream);
}

static bool AbslParseFlag(absl::string_view text, LanguageMode* mode,
                          std::string* error) {
  return kLanguageModeStringMap.Parse(text, mode, error, "--flag value");
}

static std::string AbslUnparseFlag(const LanguageMode& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

ABSL_FLAG(
    LanguageMode, lang, LanguageMode::kAutoDetect,  //
    "Selects language variant to parse.  Options:\n"
    "  auto: SystemVerilog-2017, but may auto-detect alternate parsing modes\n"
    "  sv: strict SystemVerilog-2017, with explicit alternate parsing modes\n"
    "  lib: Verilog library map language (LRM Ch. 33)\n");

ABSL_FLAG(int, error_limit, 0,
          "Limit the number of syntax errors reported.  "
          "(0: unlimited)");

ABSL_FLAG(bool, show_diagnostic_context, false,
          "prints an additional "
          "line on which the diagnostic was found,"
          "followed by a line with a position marker");

using verible::ConcreteSyntaxTree;
using verible::ParserVerifier;
using verible::TextStructureView;
using verilog::VerilogAnalyzer;

static std::unique_ptr<VerilogAnalyzer> ParseWithLanguageMode(
    absl::string_view content, absl::string_view filename) {
  switch (absl::GetFlag(FLAGS_lang)) {
    case LanguageMode::kAutoDetect:
      return VerilogAnalyzer::AnalyzeAutomaticMode(content, filename);
    case LanguageMode::kSystemVerilog: {
      auto analyzer = absl::make_unique<VerilogAnalyzer>(content, filename);
      const auto status = ABSL_DIE_IF_NULL(analyzer)->Analyze();
      if (!status.ok()) std::cerr << status.message() << std::endl;
      return analyzer;
    }
    case LanguageMode::kVerilogLibraryMap:
      return verilog::AnalyzeVerilogLibraryMap(content, filename);
  }
  return nullptr;
}

// Prints all tokens in view that are not matched in root.
static void VerifyParseTree(const TextStructureView& text_structure) {
  const ConcreteSyntaxTree& root = text_structure.SyntaxTree();
  if (root == nullptr) return;
  // TODO(fangism): this seems like a good method for TextStructureView.
  ParserVerifier verifier(*root, text_structure.GetTokenStreamView());
  auto unmatched = verifier.Verify();

  if (unmatched.empty()) {
    std::cout << std::endl << "All tokens matched." << std::endl;
  } else {
    std::cout << std::endl << "Unmatched Tokens:" << std::endl;
    for (const auto& token : unmatched) {
      std::cout << token << std::endl;
    }
  }
}

static int AnalyzeOneFile(absl::string_view content, absl::string_view filename) {
  int exit_status = 0;
  const auto analyzer = ParseWithLanguageMode(content, filename);
  const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
  const auto parse_status = analyzer->ParseStatus();

  if (!lex_status.ok() || !parse_status.ok()) {
    const std::vector<std::string> syntax_error_messages(
        analyzer->LinterTokenErrorMessages(
            absl::GetFlag(FLAGS_show_diagnostic_context)));
    const int error_limit = absl::GetFlag(FLAGS_error_limit);
    int error_count = 0;
    {
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(
              absl::GetFlag(FLAGS_show_diagnostic_context)));
      for (const auto& message : syntax_error_messages) {
        std::cout << message << std::endl;
        ++error_count;
        if (error_limit != 0 && error_count >= error_limit) break;
      }
    }
    exit_status = 1;
  }
  const bool parse_ok = parse_status.ok();

  std::function<void(std::ostream&, int)> token_translator;
  token_translator = [](std::ostream& stream, int e) {
    stream << verilog::verilog_symbol_name(e);
  };
  const verible::TokenInfo::Context context(analyzer->Data().Contents(),
                                            token_translator);

  const auto& text_structure = analyzer->Data();
  const auto& syntax_tree = text_structure.SyntaxTree();
  const verible::LineColumnMap& line_column_map = text_structure.GetLineColumnMap();
  const absl::string_view base_text = text_structure.Contents();

  {
    class AnUglyVisitor : public verible::TreeContextVisitor {
     public:
      AnUglyVisitor(const absl::string_view base_text,
                    const verible::LineColumnMap& line_column_map)
          : base_text_(base_text), line_column_map_(line_column_map) {};
      ~AnUglyVisitor() = default;

      void Visit(const verible::SyntaxTreeLeaf& leaf) override {
        VLOG(3) << __FUNCTION__ << " leaf: " << leaf.get();
        const verilog_tokentype tag = verilog_tokentype(leaf.Tag().tag);
        // store first leaf
        if (left_ == nullptr) {
          left_ = &leaf;
        }
        // track last leaf
        right_ = &leaf;
      };

      void Visit(const verible::SyntaxTreeNode& node) override {
        const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
        VLOG(3) << __FUNCTION__ << " node: " << tag;

        switch (tag) {
          case verilog::NodeEnum::kFunctionCall: {
            if (Context().IsInside(verilog::NodeEnum::kFunctionCall)) {
              VLOG(4) << "Nested function call, keeping original";
              abort_ = true;
            }
            TraverseChildren(node);
            break;
          }

          case verilog::NodeEnum::kModuleItemList: {
            const verible::SyntaxTreeContext::AutoPop p(&current_context_, &node);
            for (const auto& child : node.children()) {
              if (child) {
                const verible::ValueSaver<const verible::SyntaxTreeLeaf*>
                    left_saver(&left_, nullptr);
                const verible::ValueSaver<const verible::SyntaxTreeLeaf*>
                    right_saver(&right_, nullptr);
                const verible::ValueSaver<bool> abort_save(&abort_, false);
                child->Accept(this);

                if (abort_) {
                  int left_line = 0;
                  int right_line = 0;

                  if (left_ != nullptr) {
                    const auto& token = left_->get();
                    const auto left = line_column_map_(token.left(base_text_));
                    VLOG(4) << "    at " << left;
                    left_line = left.line + 1;
                  }
                  if (right_ != nullptr) {
                    const auto& token = right_->get();
                    const auto right = line_column_map_(token.left(base_text_));
                    VLOG(4) << "    at " << right;
                    right_line = right.line + 1;
                  }

                  if (left_line > 0 && right_line > 0) {
                    VLOG(4) << "Exclude: " << left_line << ":" << right_line;
                    ranges_.push_back(std::make_pair(left_line, right_line));
                  }
                }
              }
            }
            break;
          }

          default: {
            TraverseChildren(node);
            break;
          }
        }
      };

      const std::vector<std::pair<int, int>> GetRanges() {
        return ranges_;
      }

     private:
      void TraverseChildren(const verible::SyntaxTreeNode& node) {
        // verible::TreeUnwrapper::TraverseChildren(node)
        const verible::SyntaxTreeContext::AutoPop p(&current_context_, &node);
        for (const auto& child : node.children()) {
          if (child) {
            child->Accept(this);
          }
        }
      }


      const absl::string_view base_text_;
      const verible::LineColumnMap& line_column_map_;

      /// ////////
      bool abort_ = false;
      const verible::SyntaxTreeLeaf* left_  = nullptr;
      const verible::SyntaxTreeLeaf* right_ = nullptr;

      std::vector<std::pair<int,int>> ranges_;
    };

    AnUglyVisitor visitor(base_text, line_column_map);
    syntax_tree->Accept(&visitor);


    const auto lines_of_code = line_column_map(line_column_map.EndOffset()).line;
    VLOG(4) << "lines_of_code: " << lines_of_code;

    int itr_range = 1;
    std::vector<std::pair<int, int>> ranges;
    if (visitor.GetRanges().size() > 0) {
      for (const auto& itr : visitor.GetRanges()) {
        VLOG(4) << "ranges: " << itr.first << ":" << itr.second;
        ranges.push_back(std::make_pair(itr_range, itr.first - 1));
        itr_range = itr.second + 1;
      }
      if (itr_range < lines_of_code) {
        ranges.push_back(std::make_pair(itr_range, lines_of_code));
      }
    } else {
      ranges.push_back(std::make_pair(1, lines_of_code));
    }

    std::stringstream ss;
    for (const auto& itr : ranges) {
      VLOG(1) << "reversed ranges: " << itr.first << ":" << itr.second;
      ss << itr.first << "-" << itr.second << ",";
    }
    absl::string_view sv(ss.str());
    std::cout << sv.substr(0, sv.length()-1) << std::endl;
  }

  return exit_status;
}

int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  int exit_status = 0;
  // All positional arguments are file names.  Exclude program name.
  for (const auto filename :
       verible::make_range(args.begin() + 1, args.end())) {
    std::string content;
    if (!verible::file::GetContents(filename, &content).ok()) {
      exit_status = 1;
      continue;
    }

    int file_status = AnalyzeOneFile(content, filename);
    exit_status = std::max(exit_status, file_status);
  }

  return exit_status;
}
