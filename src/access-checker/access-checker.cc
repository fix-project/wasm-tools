/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "access-checker.h"

#include <cassert>
#include <cstdio>

#include "wabt/cast.h"
#include "wabt/expr-visitor.h"
#include "wabt/ir.h"
#include "wabt/wast-lexer.h"

namespace wabt {

namespace {

class MemoryAccessChecker : public ExprVisitor::DelegateNop {
 public:
  MemoryAccessChecker(Script* script, Errors* errors, Index rw_idx);

  Result VisitModule(Module* module);
  Result VisitScript(Script* script);

  // Implementation of ExprVisitor::DelegateNop.
  Result OnLoadExpr(LoadExpr*) override;
  Result OnMemoryCopyExpr(MemoryCopyExpr*) override;
  Result OnMemoryFillExpr(MemoryFillExpr*) override;
  Result OnMemoryGrowExpr(MemoryGrowExpr*) override;
  Result OnMemoryInitExpr(MemoryInitExpr*) override;
  Result OnMemorySizeExpr(MemorySizeExpr*) override;
  Result OnStoreExpr(StoreExpr*) override;

 private:
  void VisitFunc(Func* func);
  void VisitExport(Export* export_);
  void VisitGlobal(Global* global);
  void VisitTag(Tag* tag);
  void VisitElemSegment(ElemSegment* segment);
  void VisitDataSegment(DataSegment* segment);
  void VisitScriptModule(ScriptModule* script_module);
  void VisitCommand(Command* command);
  Result CheckMemoryAccess(Var* memidx);

  Errors* errors_ = nullptr;
  Script* script_ = nullptr;
  Module* current_module_ = nullptr;
  Func* current_func_ = nullptr;
  Index rw_idx_;
  ExprVisitor visitor_;
  std::vector<std::string> labels_;
  Result result_ = Result::Ok;
};

MemoryAccessChecker::MemoryAccessChecker(Script* script, Errors* errors, Index rw_idx)
    : errors_(errors),
      script_(script),
      rw_idx_(rw_idx),
      visitor_(this) {}

Result MemoryAccessChecker::OnLoadExpr(LoadExpr* expr) {
  return Result::Ok;
}

Result MemoryAccessChecker::OnMemoryCopyExpr(MemoryCopyExpr* expr) {
  return CheckMemoryAccess(&expr->destmemidx);
}

Result MemoryAccessChecker::OnMemoryFillExpr(MemoryFillExpr* expr) {
  return CheckMemoryAccess(&expr->memidx);
}

Result MemoryAccessChecker::OnMemoryGrowExpr(MemoryGrowExpr* expr) {
  return CheckMemoryAccess(&expr->memidx);
}

Result MemoryAccessChecker::OnMemoryInitExpr(MemoryInitExpr* expr) {
  return CheckMemoryAccess(&expr->memidx);
}

Result MemoryAccessChecker::OnMemorySizeExpr(MemorySizeExpr* expr) {
  return Result::Ok;
}

Result MemoryAccessChecker::OnStoreExpr(StoreExpr* expr) {
  return CheckMemoryAccess(&expr->memidx);
}

void MemoryAccessChecker::VisitFunc(Func* func) {
  current_func_ = func;
  visitor_.VisitFunc(func);
  current_func_ = nullptr;
}

void MemoryAccessChecker::VisitExport(Export* export_) {
  return;
}

void MemoryAccessChecker::VisitGlobal(Global* global) {
  visitor_.VisitExprList(global->init_expr);
}

void MemoryAccessChecker::VisitTag(Tag* tag) {
  return;
}

void MemoryAccessChecker::VisitElemSegment(ElemSegment* segment) {
  visitor_.VisitExprList(segment->offset);
}

void MemoryAccessChecker::VisitDataSegment(DataSegment* segment) {
  visitor_.VisitExprList(segment->offset);
}

Result MemoryAccessChecker::VisitModule(Module* module) {
  current_module_ = module;

  for (Func* func : module->funcs)
    VisitFunc(func);
  for (Export* export_ : module->exports)
    VisitExport(export_);
  for (Global* global : module->globals)
    VisitGlobal(global);
  for (Tag* tag : module->tags)
    VisitTag(tag);
  for (ElemSegment* elem_segment : module->elem_segments)
    VisitElemSegment(elem_segment);
  for (DataSegment* data_segment : module->data_segments)
    VisitDataSegment(data_segment);
  current_module_ = nullptr;
  return result_;
}

Result MemoryAccessChecker::CheckMemoryAccess(Var* memidx) {
  if (current_module_->GetMemoryIndex(*memidx) < this->rw_idx_) {
    return Result::Error;
  } 
  return Result::Ok;
}

void MemoryAccessChecker::VisitScriptModule(ScriptModule* script_module) {
  if (auto* tsm = dyn_cast<TextScriptModule>(script_module)) {
    VisitModule(&tsm->module);
  }
}

}  // end anonymous namespace

Result RebaseIndexModule(Module* module, Errors* errors, Index rw_idx) {
  MemoryAccessChecker checker(nullptr, errors, rw_idx);
  return checker.VisitModule(module);
}

}  // namespace wabt
