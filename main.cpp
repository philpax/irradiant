#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Preprocessor.h"

#include <iostream>
#include <sstream>
#include <fstream>

using namespace clang;
using namespace clang::tooling;

namespace cl = llvm::cl;

static cl::opt<bool> BakeIncludes("bake-includes", cl::init(false), cl::NotHidden,
    cl::desc("Controls whether includes should be baked into the resulting script."));

std::string EscapeString(std::string const& input)
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

void IncludeFile(std::string const& path)
{
    if (BakeIncludes)
    {
        std::fstream file(path);

        if (file.fail())
        {
            std::cout << "-- Failed to read file: " << path << "\n";
            return;
        }

        std::string str;

        file.seekg(0, std::ios::end);
        str.resize(file.tellg());
        file.seekg(0, std::ios::beg);

        file.read(&str[0], str.size());

        std::cout << "-- " << path << "\n";
        std::cout << str;
    }
    else
    {
        std::cout << "dofile \"" << path << "\"\n";
    }
}

template <typename T>
T* RecurseUntilType(Stmt* stmt)
{
    if (isa<T>(stmt))
        return cast<T>(stmt);

    for (auto child : stmt->children())
    {
        auto ret = RecurseUntilType<T>(child);
        if (ret)
            return ret;
    }

    return nullptr;
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

    void TraverseCondition(Stmt* stmt)
    {
        handlingAssignmentInCondition = true;
        TraverseStmt(stmt);
        handlingAssignmentInCondition = false;
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
                if (!stmt || isa<NullStmt>(stmt))
                    continue;

                WriteDepth();
                // HACK: Fix precedence issues with closures being called straight away
                if (isa<UnaryOperator>(stmt))
                    std::cout << ";";

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
            TraverseCondition(ifStmt->getCond());
            std::cout << " then\n";

            TraverseNewScope(ifStmt->getThen());

            WriteDepth();
            auto elseStmt = ifStmt->getElse();
            if (elseStmt)
            {
                std::cout << "else";
                if (isa<IfStmt>(elseStmt))
                {
                    TraverseStmt(elseStmt);
                }
                else
                {
                    std::cout << "\n";
                    TraverseNewScope(elseStmt);

                    WriteDepth();
                    std::cout << "end";
                }
            }
            else
            {
                std::cout << "end";
            }
            return true;
        }

        if (auto switchStmt = dyn_cast<SwitchStmt>(stmt))
        {
            // Save counter in the event we have nested switches
            auto currentCounter = counter;
            std::cout << "local _switchTempTable" << currentCounter << " = {}\n";
            Stmt* defaultStmtBody = nullptr;

            if (auto compoundStmt = dyn_cast<CompoundStmt>(switchStmt->getBody()))
            {
                bool first = true;
                for (auto stmt : compoundStmt->body())
                {
                    if (isa<CaseStmt>(stmt))
                        TraverseStmt(stmt);
                    else if (auto defaultStmt = dyn_cast<DefaultStmt>(stmt))
                        defaultStmtBody = defaultStmt->getSubStmt();
                }
            }

            WriteDepth();
            std::cout << "if _switchTempTable" << currentCounter << "[";
            TraverseStmt(switchStmt->getCond());
            std::cout << "] ~= nil then\n";

            ++depth;
            WriteDepth();
            std::cout << "_switchTempTable" << currentCounter << "[";
            TraverseStmt(switchStmt->getCond());
            std::cout << "]()\n";
            --depth;

            if (defaultStmtBody)
            {
                WriteDepth();
                std::cout << "else\n";
                TraverseNewScope(defaultStmtBody);
            }

            WriteDepth();
            std::cout << "end";
            ++counter;

            return true;
        }

        if (auto caseStmt = dyn_cast<CaseStmt>(stmt))
        {
            // Save counter in the event we have nested switches
            auto currentCounter = counter;
            auto body = caseStmt->getSubStmt();

            Expr* prevCaseLHS = nullptr;

            if (isa<CaseStmt>(body))
            {
                TraverseStmt(body);
                prevCaseLHS = dyn_cast<CaseStmt>(body)->getLHS();
            }

            WriteDepth();
            std::cout << "_switchTempTable" << currentCounter << "[";
            TraverseStmt(caseStmt->getLHS());
            std::cout << "] = ";

            if (!isa<CaseStmt>(body))
            {
                std::cout << "function()\n";
                TraverseNewScope(body);
                WriteDepth();
                std::cout << "end";
            }
            else if (prevCaseLHS)
            {
                std::cout << "_switchTempTable" << currentCounter << "[";
                TraverseStmt(prevCaseLHS);
                std::cout << "]";
            }
            else
            {
                // Unhandled catch-all
                std::cout << "function() end";
            }

            std::cout << "\n";

            return true;
        }

        if (auto doStmt = dyn_cast<DoStmt>(stmt))
        {
            std::cout << "repeat\n";

            TraverseNewScope(doStmt->getBody());

            WriteDepth();
            std::cout << "until not (";
            TraverseCondition(doStmt->getCond());
            std::cout << ")";
            return true;
        }

        if (auto whileStmt = dyn_cast<WhileStmt>(stmt))
        {
            std::cout << "while ";
            TraverseCondition(whileStmt->getCond());
            std::cout << " do\n";

            TraverseNewScope(whileStmt->getBody());

            WriteDepth();
            std::cout << "end";
            return true;
        }

        // Lower for loops to while loops, because C is a wonderful language
        // in which anything can happen in a for loop's expressions >_>
        if (auto forStmt = dyn_cast<ForStmt>(stmt))
        {
            TraverseStmt(forStmt->getInit());
            std::cout << "\n";

            WriteDepth();
            std::cout << "while ";
            TraverseCondition(forStmt->getCond());
            std::cout << " do\n";

            TraverseNewScope(forStmt->getBody());
            ++depth;
            WriteDepth();
            std::cout << ";";
            TraverseStmt(forStmt->getInc());
            --depth;
            std::cout << "\n";

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

        if (auto unaryOperator = dyn_cast<UnaryOperator>(stmt))
        {
            auto opcode = unaryOperator->getOpcode();
            switch (opcode)
            {
            case UO_Minus:
                std::cout << "-";
                TraverseStmt(unaryOperator->getSubExpr());
                break;
            case UO_Not:
                std::cout << "bit._not(";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << ")";
                break;
            case UO_LNot:
                std::cout << "not ";
                TraverseStmt(unaryOperator->getSubExpr());
                break;
            case UO_Deref:
            {
                // Special case based on RHS type
                auto declRefExpr = RecurseUntilType<DeclRefExpr>(unaryOperator->getSubExpr());
                if (!declRefExpr)
                    break;

                auto decl = declRefExpr->getDecl();
                auto& type = *decl->getType();
                if (type.isFunctionPointerType())
                {
                    // Emit the decl: no dereferencing required
                    std::cout << decl->getNameAsString();
                }
                break;
            }
            // Lower pre/post operators to functions
            // Pre: (function() expr = expr OP 1; return expr)()
            // Post: (function() local _ = expr; expr = expr OP 1; return _)()
            case UO_PreDec:
                std::cout << "(function() ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " - 1; return ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " end)()";
                break;
            case UO_PreInc:
                std::cout << "(function() ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " + 1; return ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " end)()";
                break;
            case UO_PostDec:
                std::cout << "(function() local _ = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << "; ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " - 1; return _ end)()";
                break;
            case UO_PostInc:
                std::cout << "(function() local _ = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << "; ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " = ";
                TraverseStmt(unaryOperator->getSubExpr());
                std::cout << " + 1; return _ end)()";
                break;
            default:
                break;
            }
            return true;
        }

        if (auto binaryOperator = dyn_cast<BinaryOperator>(stmt))
        {
            if (binaryOperator->isAssignmentOp() && handlingAssignmentInCondition)
            {
                std::cout << "(function() ";
            }

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
                std::cout << " ~= ";
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

            if (binaryOperator->isAssignmentOp() && handlingAssignmentInCondition)
            {
                std::cout << "; return ";
                TraverseStmt(binaryOperator->getLHS());
                std::cout << " end)()";
            }

            return true;
        }

        // Lower ternary operator to a closure
        if (auto conditionalOperator = dyn_cast<ConditionalOperator>(stmt))
        {
            std::cout << "(function() if ";
            TraverseCondition(conditionalOperator->getCond());
            std::cout << " then return ";
            TraverseStmt(conditionalOperator->getTrueExpr());
            std::cout << " else return ";
            TraverseStmt(conditionalOperator->getFalseExpr());
            std::cout << " end end)()";
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
            std::cout << "end\n\n";
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
            std::cout << '"' << EscapeString(stringLiteral->getString().str()) << '"';
            return true;
        }

        if (auto characterLiteral = dyn_cast<CharacterLiteral>(stmt))
        {
            // Unlikely to be portable, but works well enough for our purposes
            auto value = static_cast<wchar_t>(characterLiteral->getValue());
            std::wcout << L"string.byte(\"" << value << L"\")";
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
    bool handlingAssignmentInCondition = false;
    uint32_t counter = 0;
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
        auto entry = sourceManager.getFileEntryForID(sourceManager.getMainFileID());
        std::cout << "-- main file\n";

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
        IncludeFile("shim/lua/bitwise.lua");

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

                IncludeFile(newFileName);
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
