//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UsePlaceholderBindingCheck.h"
#include "../utils/DeclRefExprUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

namespace {
AST_POLYMORPHIC_MATCHER(isInMacro,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(Stmt, Decl)) {
  return Node.getBeginLoc().isMacroID() || Node.getEndLoc().isMacroID();
}

AST_MATCHER(VarDecl, hasAutomaticStorageDurationAndNoSpecifiers) {
  return !Node.isStaticLocal() && !Node.isConstexpr() && !Node.hasAttrs() &&
         Node.getStorageClass() == SC_None &&
         Node.getTSCSpec() == TSCS_unspecified;
}
} // namespace

void UsePlaceholderBindingCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      decompositionDecl(unless(isInMacro()),
                        hasAutomaticStorageDurationAndNoSpecifiers(),
                        hasAncestor(compoundStmt().bind("scope")))
          .bind("decomp"),
      this);
}

void UsePlaceholderBindingCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Decomp = Result.Nodes.getNodeAs<DecompositionDecl>("decomp");
  const auto *Scope = Result.Nodes.getNodeAs<CompoundStmt>("scope");
  ASTContext &Context = *Result.Context;

  for (const BindingDecl *Binding : Decomp->bindings()) {
    if (Binding->isParameterPack() ||
        Binding->isPlaceholderVar(Context.getLangOpts()))
      continue;

    const llvm::SmallPtrSet<const DeclRefExpr *, 16> Refs =
        utils::decl_ref_expr::allDeclRefExprs(*Binding, *Scope, Context);
    if (Refs.size() != 1)
      continue;

    const DeclRefExpr *Ref = *Refs.begin();

    const DynTypedNodeList RefParents = Context.getParents(*Ref);
    if (RefParents.size() != 1)
      continue;
    const auto *Cast =
        dyn_cast_or_null<CStyleCastExpr>(RefParents[0].get<Expr>());
    if (!Cast || !Cast->getType()->isVoidType() ||
        Cast->getSubExpr()->IgnoreParens() != Ref ||
        Cast->getBeginLoc().isMacroID() || Cast->getEndLoc().isMacroID())
      continue;

    const DynTypedNodeList CastParents = Context.getParents(*Cast);
    if (CastParents.size() != 1 || !CastParents[0].get<CompoundStmt>())
      continue;

    const SourceLocation SemiLoc = Lexer::findLocationAfterToken(
        Cast->getEndLoc(), tok::semi, Context.getSourceManager(),
        Context.getLangOpts(), /*SkipTrailingWhitespaceAndNewLine=*/true);
    if (SemiLoc.isInvalid())
      continue;

    diag(Binding->getLocation(),
         "binding %0 is only used to suppress an unused variable warning; "
         "use a placeholder '_' instead")
        << Binding
        << FixItHint::CreateReplacement(Binding->getSourceRange(), "_")
        << FixItHint::CreateRemoval(
               CharSourceRange::getCharRange(Cast->getBeginLoc(), SemiLoc));
  }
}

} // namespace clang::tidy::modernize
