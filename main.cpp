#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::tooling;

class HelloVisitor : public RecursiveASTVisitor<HelloVisitor> {
public:
    explicit HelloVisitor(ASTContext *Context, Rewriter &R) : Context(Context), TheRewriter(R) {}

    bool VisitFunctionDecl(FunctionDecl *Func) {
        if (Context->getSourceManager().isInMainFile(Func->getLocation()) && Func->hasBody()) {
            Stmt *Body = Func->getBody();
            SourceLocation StartLoc = Body->getBeginLoc().getLocWithOffset(1); // Inside the '{'

            std::string Insertion = "\nprintf(\"Entered function: " + Func->getNameAsString() + "\\n\");\n";
            TheRewriter.InsertText(StartLoc, Insertion, true, true);
        }
        return true;
    }

private:
    ASTContext *Context;
    Rewriter &TheRewriter;
};

class HelloConsumer : public ASTConsumer {
    HelloVisitor Visitor;
public:
    HelloConsumer(ASTContext *Context, Rewriter &R) : Visitor(Context, R) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};


class HelloFrontendAction : public ASTFrontendAction {
public:
void EndSourceFileAction() override {
    const SourceManager &SM = TheRewriter.getSourceMgr();
    const FileEntry *Entry = SM.getFileEntryForID(SM.getMainFileID());

    if (!Entry) {
        llvm::errs() << "Could not get FileEntry\n";
        return;
    }

    std::string OriginalFile = Entry->getName().str();
    std::string OutputFile = OriginalFile.substr(0, OriginalFile.find_last_of('.')) + "_parsed.cpp";

    std::error_code EC;
    llvm::raw_fd_ostream OutFile(OutputFile, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "Could not open output file: " << EC.message() << "\n";
        return;
    }

    TheRewriter.getEditBuffer(SM.getMainFileID()).write(OutFile);
    llvm::outs() << "Modified file written to: " << OutputFile << "\n";
}


    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<HelloConsumer>(&CI.getASTContext(), TheRewriter);
    }

private:
    Rewriter TheRewriter;
};


static llvm::cl::OptionCategory MyToolCategory("hello-tool options");

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << "Error creating CommonOptionsParser\n";
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();

    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    return Tool.run(newFrontendActionFactory<HelloFrontendAction>().get());
}
