#!/bin/bash
# Copyright 2017-2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Find input files
MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_INPUT_FILE
MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE
MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"
readonly MY_EXPECT_FILE

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-project path."
  exit 1
}
project_tool="$1"

################################################################################
echo "=== Test no arguments."

"$project_tool" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Test '--helpfull'."

"$project_tool" --helpfull > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

grep -q "file_list_path" "$MY_OUTPUT_FILE" || {
  echo "Expected \"file_list_path\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Expect failure on missing required flag"

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-defs \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

grep -q "file_list_path is required but missing" "$MY_OUTPUT_FILE" || {
  echo "Expected \"file_list_path is required but missing\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Expect failure on nonexistent file list"

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-defs \
  --file_list_path "${TEST_TMPDIR}/nonexistent.txt" \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Expect failure on nonexistent file listed in the file list"

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-refs \
  --file_list_path <(echo "nonexistent.sv") \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

# Make sure we see some diagnostic message about missing files.
grep -q "No such file" "$MY_OUTPUT_FILE" || {
  echo "Expected \"No such file\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Load one file, printing symbol table for debug."

cat > "$MY_INPUT_FILE" <<EOF
localparam int fooo = 1;
localparam int barr = fooo;
EOF

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-defs \
  --file_list_path <(echo myinput.txt) \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "metatype: parameter" "$MY_OUTPUT_FILE" || {
  echo "Expected \"metatype: parameter\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Load one file containing syntax error (build-only)."

cat > "$MY_INPUT_FILE" <<EOF
localparam 777;
EOF

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-defs \
  --file_list_path <(echo myinput.txt) \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

grep -q "[combined statuses]:" "$MY_OUTPUT_FILE" || {
  echo "Expected \"[combined statuses]:\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Load one file, printing symbol references for debug."

cat > "$MY_INPUT_FILE" <<EOF
localparam int fooo = 1;
localparam int barr = fooo;
EOF

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-refs \
  --file_list_path <(echo myinput.txt) \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "(@fooo -> \$root::fooo)" "$MY_OUTPUT_FILE" || {
  echo "Expected \"(@fooo -> \$root::fooo)\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Load one file containing syntax error (build-and-resolve)."

cat > "$MY_INPUT_FILE" <<EOF
localparam 777;
EOF

 # Construct a file-list on-the-fly as a file-descriptor
"$project_tool" \
  symbol-table-refs \
  --file_list_path <(echo myinput.txt) \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

grep -q "[combined statuses]:" "$MY_OUTPUT_FILE" || {
  echo "Expected \"[combined statuses]:\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "PASS"
