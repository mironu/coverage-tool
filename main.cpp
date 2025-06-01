#include <clang/AST/AST.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm/Support/CommandLine.h>

#include <filesystem>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ToolCategory("coverage-tool");
static cl::opt<std::string> BuildPath("p", cl::desc("Build path"), cl::value_desc("path"), cl::Required, cl::cat(ToolCategory));

class CoverageVisitor : public RecursiveASTVisitor<CoverageVisitor> {
public:
    explicit CoverageVisitor(Rewriter &R, ASTContext &C)
        : TheRewriter(R), Context(C) {}

    bool VisitFunctionDecl(FunctionDecl *Func) {
        if (Context.getSourceManager().isInMainFile(Func->getLocation()) && Func->hasBody()) {
            Stmt *Body = Func->getBody();
            SourceLocation StartLoc = Body->getBeginLoc().getLocWithOffset(1); // after '{'

            std::string funcName = Func->getNameAsString();
            unsigned line = Context.getSourceManager().getSpellingLineNumber(Func->getLocation());
            std::string id = funcName + ":" + std::to_string(line);
            std::string insertionText = "\n    __cov_hit(\"" + id + "\");\n";

            TheRewriter.InsertText(StartLoc, insertionText, true, true);
        }
        return true;
    }

private:
    Rewriter &TheRewriter;
    ASTContext &Context;
};

class CoverageConsumer : public ASTConsumer {
public:
    explicit CoverageConsumer(Rewriter &R, ASTContext &C)
        : Visitor(R, C) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    CoverageVisitor Visitor;
};

class CoverageAction : public ASTFrontendAction {
public:
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();

        std::string filename = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        std::filesystem::path inputPath(filename);
        std::filesystem::path outputDir = "instrumented";

        std::filesystem::create_directories(outputDir); // Ensure output dir exists

        std::string outFile =
        (outputDir / (inputPath.stem().string() + "_parsed" + inputPath.extension().string())).string();

        std::error_code EC;
        llvm::raw_fd_ostream out(outFile, EC);
        TheRewriter.getEditBuffer(SM.getMainFileID()).write(out);
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<CoverageConsumer>(TheRewriter, CI.getASTContext());
    }

private:
    Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
    cl::HideUnrelatedOptions(ToolCategory);
    cl::ParseCommandLineOptions(argc, argv, "Coverage Tool\n");

    std::string ErrorMessage;
    std::unique_ptr<CompilationDatabase> Compilations =
        CompilationDatabase::loadFromDirectory(BuildPath, ErrorMessage);

    if (!Compilations) {
        errs() << "Error loading compilation database: " << ErrorMessage << "\n";
        return 1;
    }

    // Get all source files from the compilation database
    std::vector<std::string> allFiles;
    for (const auto &command : Compilations->getAllCompileCommands()) {
        allFiles.push_back(command.Filename);
    }

    if (allFiles.empty()) {
        errs() << "No source files found in the compilation database.\n";
        return 1;
    }

    ClangTool Tool(*Compilations, allFiles);
    return Tool.run(newFrontendActionFactory<CoverageAction>().get());
}