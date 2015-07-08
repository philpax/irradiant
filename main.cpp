#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Preprocessor.h"

#include <iostream>
#include <sstream>

using namespace clang;
using namespace clang::tooling;

std::string escapeString(std::string const& input)
{
    std::ostringstream ss;
    for (auto c : input)
    {
        switch (c)
        {
        case '\\':
            ss << "\\\\";
            break;
        case '"':
            ss << "\\\"";
            break;
        case '/':
            ss << "\\/";
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
            ss << c;
            break;
        }
    }
    return ss.str();
}

class DumpVisitor : public RecursiveASTVisitor<DumpVisitor>
{
  public:
    explicit DumpVisitor(ASTContext* context) : context(context) {}

    void TraverseNewScope(Stmt* stmt)
    {
        if (!isa<CompoundStmt>(stmt))
        {
            depth++;
            WriteDepth();
        }

        TraverseStmt(stmt);

        if (!isa<CompoundStmt>(stmt))
        {
            std::cout << "\n";
            depth--;
        }
    }

    bool TraverseStmt(Stmt* stmt)
    {
        if (!stmt)
            return RecursiveASTVisitor::TraverseStmt(stmt);

        if (auto compoundStmt = dyn_cast<CompoundStmt>(stmt))
        {
            depth++;
            for (auto stmt : compoundStmt->body())
            {
                if (!stmt)
                    continue;

                WriteDepth();
                TraverseStmt(stmt);
                std::cout << "\n";
            }
            depth--;
            return true;
        }

        if (auto callExpr = dyn_cast<CallExpr>(stmt))
        {
            TraverseStmt(callExpr->getCallee());
            bool first = true;
            std::cout << "(";
            for (auto param : callExpr->arguments())
            {
                if (!first)
                    std::cout << ", ";

                TraverseStmt(param);
                first = false;
            }
            std::cout << ")";
            return true;
        }

        if (auto ifStmt = dyn_cast<IfStmt>(stmt))
        {
            std::cout << "if ";
            TraverseStmt(ifStmt->getCond());
            std::cout << " then\n";

            TraverseNewScope(ifStmt->getThen());

            WriteDepth();
            std::cout << "end";
            return true;
        }

        if (auto arraySubscriptExpr = dyn_cast<ArraySubscriptExpr>(stmt))
        {
            TraverseStmt(arraySubscriptExpr->getBase());
            std::cout << "[(";
            TraverseStmt(arraySubscriptExpr->getIdx());
            // Add 1 to compensate for 1-based indexing
            std::cout << ")+1]";
            return true;
        }

        if (auto binaryOperator = dyn_cast<BinaryOperator>(stmt))
        {
            if (binaryOperator->isCompoundAssignmentOp())
            {
                TraverseStmt(binaryOperator->getLHS());
                std::cout << " = ";
            }

            auto opcode = binaryOperator->getOpcode();

            // Lua doesn't have native bitwise operators, so these need to be lowered
            // to function calls
            bool normalBinaryOperator = false;
            switch (opcode)
            {
            case BO_Shl:
            case BO_ShlAssign:
                std::cout << "bit._shl(";
                break;
            case BO_Shr:
            case BO_ShrAssign:
                std::cout << "bit._shr(";
                break;
            case BO_And:
            case BO_AndAssign:
                std::cout << "bit._and(";
                break;
            case BO_Xor:
            case BO_XorAssign:
                std::cout << "bit._xor(";
                break;
            case BO_Or:
            case BO_OrAssign:
                std::cout << "bit._or(";
                break;
            default:
                normalBinaryOperator = true;
                break;
            }

            if (!normalBinaryOperator)
            {
                TraverseStmt(binaryOperator->getLHS());
                std::cout << ", ";
                TraverseStmt(binaryOperator->getRHS());
                std::cout << ")";
                return true;
            }

            TraverseStmt(binaryOperator->getLHS());
            switch (opcode)
            {
            case BO_Mul:
            case BO_MulAssign:
                std::cout << " * ";
                break;
            case BO_Div:
            case BO_DivAssign:
                std::cout << " / ";
                break;
            case BO_Rem:
            case BO_RemAssign:
                std::cout << " % ";
                break;
            case BO_Add:
            case BO_AddAssign:
                std::cout << " + ";
                break;
            case BO_Sub:
            case BO_SubAssign:
                std::cout << " - ";
                break;
            case BO_LT:
                std::cout << " < ";
                break;
            case BO_GT:
                std::cout << " > ";
                break;
            case BO_LE:
                std::cout << " <= ";
                break;
            case BO_GE:
                std::cout << " >= ";
                break;
            case BO_EQ:
                std::cout << " == ";
                break;
            case BO_NE:
                std::cout << " != ";
                break;
            case BO_LAnd:
                std::cout << " and ";
                break;
            case BO_LOr:
                std::cout << " or ";
                break;
            case BO_Comma:
                std::cout << ", ";
                break;
            case BO_Assign:
                std::cout << " = ";
                break;
            default:
                break;
            }
            TraverseStmt(binaryOperator->getRHS());

            return true;
        }

        if (auto parenExpr = dyn_cast<ParenExpr>(stmt))
        {
            std::cout << "(";
            TraverseStmt(parenExpr->getSubExpr());
            std::cout << ")";
            return true;
        }

        if (auto declStmt = dyn_cast<DeclStmt>(stmt))
        {
            bool first = true;

            std::vector<Expr*> initExprs;
            for (auto decl : declStmt->decls())
            {
                if (first)
                    std::cout << "local ";
                else
                    std::cout << ", ";

                if (auto varDecl = dyn_cast<VarDecl>(decl))
                {
                    auto name = varDecl->getNameAsString();
                    if (name.empty())
                        continue;

                    std::cout << name;

                    if (varDecl->hasInit())
                        initExprs.push_back(varDecl->getInit());
                    else
                        initExprs.push_back(nullptr);
                }
                else
                {
                    TraverseDecl(decl);
                }
                first = false;
            }

            auto notNullPredicate = [](Expr* e) { return e != nullptr; };
            if (initExprs.size() &&
                std::any_of(initExprs.begin(), initExprs.end(), notNullPredicate))
            {
                std::cout << " = ";
                bool first = true;
                for (auto expr : initExprs)
                {
                    if (!first)
                        std::cout << ", ";

                    if (expr == nullptr)
                        std::cout << "nil";
                    else
                        TraverseStmt(expr);

                    first = false;
                }
            }

            return true;
        }

        if (auto initListExpr = dyn_cast<InitListExpr>(stmt))
        {
            bool first = true;
            std::cout << "{";
            for (auto expr : *initListExpr)
            {
                if (!first)
                    std::cout << ", ";

                TraverseStmt(expr);
                first = false;
            }
            std::cout << "}";
            return true;
        }

        return RecursiveASTVisitor::TraverseStmt(stmt);
    }

    bool TraverseDecl(Decl* decl)
    {
        if (auto functionDecl = dyn_cast<FunctionDecl>(decl))
        {
            // Skip over function declarations
            if (!functionDecl->doesThisDeclarationHaveABody())
                return true;

            if (functionDecl->isMain())
                foundMain = true;

            std::cout << "\n";

            WriteDepth();
            std::cout << "function " << functionDecl->getNameAsString();
            std::cout << "(";
            bool first = true;
            for (auto param : functionDecl->params())
            {
                if (!first)
                    std::cout << ", ";

                std::cout << param->getNameAsString();
                first = false;
            }
            std::cout << ")";
            std::cout << "\n";

            if (functionDecl->hasBody())
                TraverseStmt(functionDecl->getBody());

            WriteDepth();
            std::cout << "end\n";
            return true;
        }

        // Duplicated from TraverseStmt: required to catch
        // unhandled var decls (see: global scope)
        if (auto varDecl = dyn_cast<VarDecl>(decl))
        {
            auto name = varDecl->getNameAsString();
            if (name.empty())
                return true;

            std::cout << name;

            if (varDecl->hasInit())
            {
                std::cout << " = ";
                TraverseStmt(varDecl->getInit());
            }

            return true;
        }

        return RecursiveASTVisitor::TraverseDecl(decl);
    }

    bool VisitStmt(Stmt* stmt)
    {
        if (auto stringLiteral = dyn_cast<StringLiteral>(stmt))
        {
            std::cout << '"' << escapeString(stringLiteral->getString().str()) << '"';
            return true;
        }

        if (auto integerLiteral = dyn_cast<IntegerLiteral>(stmt))
        {
            std::cout << integerLiteral->getValue().toString(10, true);
            return true;
        }

        if (auto floatingLiteral = dyn_cast<FloatingLiteral>(stmt))
        {
            std::cout << floatingLiteral->getValueAsApproximateDouble();
            return true;
        }

        if (auto declRefExpr = dyn_cast<DeclRefExpr>(stmt))
        {
            std::cout << declRefExpr->getDecl()->getNameAsString();
            return true;
        }

        if (auto returnStmt = dyn_cast<ReturnStmt>(stmt))
        {
            std::cout << "return";
            if (returnStmt->getRetValue())
                std::cout << " ";

            return true;
        }

        return true;
    }

    void WriteDepth()
    {
        for (uint32_t i = 0; i < depth * 4; ++i)
            std::cout << ' ';
    }

    bool HasFoundMain() { return foundMain; }

  private:
    ASTContext* context;
    uint32_t depth = 0;
    bool foundMain = false;
};

class DumpConsumer : public ASTConsumer
{
  public:
    explicit DumpConsumer(CompilerInstance& compiler)
        : visitor(&compiler.getASTContext()), sourceManager(compiler.getSourceManager())
    {
    }

    virtual void HandleTranslationUnit(ASTContext& context) override
    {
        auto decls = context.getTranslationUnitDecl()->decls();
        for (auto decl : decls)
        {
            auto const& fileId = sourceManager.getFileID(decl->getLocation());
            if (fileId != sourceManager.getMainFileID())
                continue;

            visitor.TraverseDecl(decl);
        }

        // Pass args to main(), and call it
        if (visitor.HasFoundMain())
        {
            std::cout << "return (function() "
                      << "table.insert(arg, 1, arg[0]); "
                      << "return main(#arg, arg) end)()\n";
        }
    }

  private:
    DumpVisitor visitor;
    SourceManager& sourceManager;
};

class DumpAction : public ASTFrontendAction
{
  public:
    virtual bool BeginSourceFileAction(CompilerInstance& compiler, StringRef) override
    {
        std::cout << "dofile \"shim/lua/bitwise.lua\"\n";

        class PrintIncludes : public PPCallbacks
        {
        public:
            PrintIncludes(SourceManager& sourceManager)
                : sourceManager(sourceManager)
            {
            }

            virtual void InclusionDirective(SourceLocation location, Token const&, StringRef fileName,
                                            bool isAngled, CharSourceRange, FileEntry const*,
                                            StringRef, StringRef, Module const*) override
            {
                // Skip over includes that are not from the main file
                if (sourceManager.getFileID(location) != sourceManager.getMainFileID())
                    return;

                auto newFileName = fileName.str();
                newFileName = newFileName.substr(0, newFileName.find_last_of('.'));
                newFileName += ".lua";

                if (isAngled)
                    newFileName = "shim/lua/" + newFileName;

                std::cout << "dofile \"" << newFileName << "\"\n";
            }

            SourceManager& sourceManager;
        };

        std::unique_ptr<PrintIncludes> callback(
            new PrintIncludes(compiler.getSourceManager()));
        auto& preprocessor = compiler.getPreprocessor();
        preprocessor.addPPCallbacks(std::move(callback));

        return true;
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                                           llvm::StringRef inFile) override
    {
        return std::unique_ptr<ASTConsumer>(new DumpConsumer(compiler));
    }
};

int main(int argc, char const** argv)
{
    llvm::cl::OptionCategory category("irradiant");
    // Ugly hack to get around there being no easy way of adding arguments
    std::vector<char const*> arguments(argv, argv + argc);
    if (strcmp(arguments.back(), "--") == 0)
        arguments.pop_back();
    arguments.push_back("-extra-arg=-fno-builtin");
    arguments.push_back("-extra-arg=-nostdlib");
    arguments.push_back("-extra-arg=-nostdinc");
    arguments.push_back("-extra-arg=-Ishim/c");
    arguments.push_back("--");

    int size = arguments.size();
    CommonOptionsParser parser(size, arguments.data(), category);

    ClangTool tool(parser.getCompilations(), parser.getSourcePathList());
    return tool.run(newFrontendActionFactory<DumpAction>().get());
}
