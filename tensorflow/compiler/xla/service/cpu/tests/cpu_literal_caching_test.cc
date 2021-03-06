/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/service/cpu/cpu_compiler.h"
#include "tensorflow/compiler/xla/service/cpu/tests/cpu_codegen_test.h"
#include "tensorflow/compiler/xla/tools/parser/hlo_parser.h"

namespace xla {
namespace cpu {
namespace {
class CpuDuplicateConstantsTest : public CpuCodegenTest {};

TEST_F(CpuDuplicateConstantsTest, RepeatedArrayConstants) {
  // We use a while loop here to force the two constant HloInstructions to be in
  // different computations.  Otherwise the HLO optimizer itself CSEs them.
  const string hlo_text = R"(
HloModule RepeatedConstants

while_body {
  arg_body = f32[2,3,2] parameter(0)
  ROOT const = f32[2,3,2] constant(
  f32[2,3,2]
    {{{1, 2}, {1001, 1002}, {2001, 2002}},
     {{2, 1}, {2001, 3002}, {2001, 2002}}})
}

while_cond {
  arg_cond = f32[2,3,2] parameter(0)
  ROOT unknown = pred[] infeed()
}

ENTRY main {
  param = f32[2,3,2] parameter(0)
  const_a = f32[2,3,2] constant(
  f32[2,3,2]
    {{{1, 2}, {1001, 1002}, {2001, 2002}},
     {{2, 1}, {2001, 3002}, {2001, 2002}}})
  const_b = f32[2,3,2] while(f32[2,3,2] const_a), condition=while_cond, body=while_body

  out0 = () outfeed(f32[2,3,2] const_a)
  out1 = () outfeed(f32[2,3,2] const_b)

  ROOT root = f32[] constant(1)
}
)";

  // TODO(b/78879738): The fake "f32[] constant(1)" root is only needed to work
  // around b/78879738.  Once b/78879738 is fixed, we can set one of the
  // outfeeds as the root.

  string filecheck_pattern = R"(
CHECK: private constant [2 x [3 x [2 x float]]]
CHECK-NOT: private constant [2 x [3 x [2 x float]]]
)";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          tools::Parse(hlo_text));

  CpuAotCompilationOptions options{
      /*triple=*/"x86_64-pc-linux", /*cpu_name=*/"", /*features=*/"",
      /*entry_point_name=*/"entry",
      /*relocation_model=*/CpuAotCompilationOptions::RelocationModel::Static};

  CompileAheadOfTimeAndVerifyIr(std::move(module), options, filecheck_pattern,
                                /*match_optimized_ir=*/false);
}

TEST_F(CpuDuplicateConstantsTest, RepeatedTupleConstants) {
  // We use a while loop here to force the two constant HloInstructions to be in
  // different computations.  Otherwise the HLO optimizer itself CSEs them.
  const string hlo_text = R"(
HloModule RepeatedConstants

while_body {
  arg_body = (f32[2,1]{1,0}, f32[2]{0}) parameter(0)
  ROOT const = (f32[2,1]{1,0}, f32[2]{0}) constant((f32[2,1], f32[2]) ( f32[2,1] { { 1 }, { 2 } }, {2, 42} ))
}

while_cond {
  arg_cond = (f32[2,1]{1,0}, f32[2]{0}) parameter(0)
  ROOT unknown = pred[] infeed()
}

ENTRY main {
  param = f32[2,3,2] parameter(0)
  const_a = (f32[2,1]{1,0}, f32[2]{0}) constant((f32[2,1], f32[2]) ( f32[2,1] { { 1 }, { 2 } }, {2, 42} ))
  const_b = (f32[2,1]{1,0}, f32[2]{0}) while((f32[2,1]{1,0}, f32[2]{0}) const_a), condition=while_cond, body=while_body

  out0 = () outfeed((f32[2,1]{1,0}, f32[2]{0}) const_a)
  out1 = () outfeed((f32[2,1]{1,0}, f32[2]{0}) const_b)

  ROOT root = f32[] constant(1)
}
)";

  // TODO(b/78879738): The fake "f32[] constant(1)" root is only needed to work
  // around b/78879738.  Once b/78879738 is fixed, we can set one of the
  // outfeeds as the root.

  string filecheck_pattern = R"(
CHECK: private constant [2 x float]
CHECK: private constant [2 x [1 x float]]
CHECK-NOT: private constant [2 x float]
CHECK-NOT: private constant [2 x [1 x float]]
)";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          tools::Parse(hlo_text));

  CpuAotCompilationOptions options{
      /*triple=*/"x86_64-pc-linux", /*cpu_name=*/"", /*features=*/"",
      /*entry_point_name=*/"entry",
      /*relocation_model=*/CpuAotCompilationOptions::RelocationModel::Static};

  CompileAheadOfTimeAndVerifyIr(std::move(module), options, filecheck_pattern,
                                /*match_optimized_ir=*/false);
}

}  // namespace
}  // namespace cpu
}  // namespace xla
