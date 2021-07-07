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

#ifndef VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
#define VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_

#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/vector_tree.h"

namespace verilog {
namespace formatter {

void OptimizeTokenPartitionTree(verible::TokenPartitionTree* node,
                                const verible::BasicFormatStyle& style);

}  // formatter
}  // verilog

#endif  // VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
