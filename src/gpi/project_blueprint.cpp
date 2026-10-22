#include "gpi/project_blueprint.hpp"

#include "cpp20/cpp20_lexicon.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace genomic::gpi {
namespace {
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;std::size_t begin=0;
    while(begin<=line.size()) {
        const auto end=line.find('\t',begin);result.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return result;
}
bool identifier(const std::string& value) {
    if(value.empty()||Cpp20Lexicon::isKeyword(value)||!(std::isalpha(static_cast<unsigned char>(value[0]))||value[0]=='_'))return false;
    return std::all_of(value.begin()+1,value.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}
bool qualifiedIdentifier(const std::string& value) {
    std::size_t begin=0;
    while(begin<value.size()) {
        const auto end=value.find("::",begin);const auto part=value.substr(begin,end==std::string::npos?end:end-begin);
        if(!identifier(part))return false;
        if(end==std::string::npos)return true;
        begin=end+2;
    }
    return false;
}
void validateType(const std::string& type) {
    if(type.empty()||type.find_first_of(";{}#()=[]\r\n\t")!=std::string::npos)
        throw std::invalid_argument("Blueprint contains an unsafe or empty type");
    const auto tokens=Cpp20Lexicon::tokenize(type);
    for(const auto& token:tokens) {
        if(token.type==TokenType::EndOfFile||token.type==TokenType::Identifier||token.type==TokenType::Keyword||
           token.type==TokenType::ColonColon||token.type==TokenType::Less||token.type==TokenType::Greater||
           token.type==TokenType::Comma||token.type==TokenType::Star||token.type==TokenType::Amp)continue;
        throw std::invalid_argument("Blueprint type contains a forbidden token");
    }
}
void validateExpression(const std::string& expression) {
    if(expression.empty())throw std::invalid_argument("Blueprint expression must not be empty");
    const auto tokens=Cpp20Lexicon::tokenize(expression);int parentheses=0,brackets=0;
    for(const auto& token:tokens) {
        if(token.type==TokenType::Semicolon||token.type==TokenType::OpenBrace||token.type==TokenType::CloseBrace||
           token.type==TokenType::Hash||token.type==TokenType::HashHash||
           (token.type>=TokenType::PpDefine&&token.type<=TokenType::PpError))
            throw std::invalid_argument("Blueprint expression contains a statement or preprocessor token");
        if(token.type==TokenType::OpenParen)++parentheses;
        if(token.type==TokenType::CloseParen&&--parentheses<0)
            throw std::invalid_argument("Blueprint expression has unbalanced parentheses");
        if(token.type==TokenType::OpenBracket)++brackets;
        if(token.type==TokenType::CloseBracket&&--brackets<0)
            throw std::invalid_argument("Blueprint expression has unbalanced brackets");
    }
    if(parentheses||brackets)throw std::invalid_argument("Blueprint expression has unbalanced delimiters");
}
std::filesystem::path safeFixture(const std::filesystem::path& root,const std::string& relative) {
    const std::filesystem::path input(relative);if(input.empty()||input.is_absolute())throw std::invalid_argument("Blueprint fixture path must be relative");
    const auto path=(root/input).lexically_normal();const auto lexical=path.lexically_relative(root);
    if(lexical.empty()||*lexical.begin()==".."||!std::filesystem::is_regular_file(path))
        throw std::invalid_argument("Blueprint fixture is missing or escapes its root");
    const auto canonicalRoot=std::filesystem::canonical(root),canonicalPath=std::filesystem::canonical(path);
    const auto resolved=canonicalPath.lexically_relative(canonicalRoot);
    if(resolved.empty()||*resolved.begin()=="..")throw std::invalid_argument("Blueprint fixture resolves outside its root");
    return path;
}
std::string fixture(const std::filesystem::path& root,const std::string& relative) {
    if(relative=="-")return {};
    std::ifstream stream(safeFixture(root,relative),std::ios::binary);
    if(!stream)throw std::runtime_error("Cannot read blueprint fixture");
    const std::string result{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    if(stream.bad())throw std::runtime_error("Blueprint fixture read failure");
    return result;
}
int integer(const std::string& value) {
    int result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Blueprint integer is invalid");
    return result;
}
std::string parameterList(const BlueprintFunction& function,bool names) {
    std::ostringstream out;
    for(std::size_t index=0;index<function.parameters.size();++index) {
        if(index)out<<", ";
        out<<function.parameters[index].type;
        if(names)out<<' '<<function.parameters[index].name;
    }
    return out.str();
}
void validateBlueprint(const ProjectBlueprint& blueprint) {
    if(!identifier(blueprint.projectName)||!qualifiedIdentifier(blueprint.namespaceName)||blueprint.functions.empty()||
       blueprint.entryExpression.empty()||blueprint.tests.empty())throw std::invalid_argument("Blueprint lacks required typed project elements");
    validateType(blueprint.entryType);validateExpression(blueprint.entryExpression);
    std::unordered_set<std::string> names;
    for(const auto& function:blueprint.functions) {
        if(!identifier(function.name)||!names.insert(function.name).second)throw std::invalid_argument("Blueprint function names must be unique identifiers");
        validateType(function.returnType);validateExpression(function.expression);std::unordered_set<std::string> parameters;
        for(const auto& parameter:function.parameters) {
            validateType(parameter.type);if(!identifier(parameter.name)||!parameters.insert(parameter.name).second)
                throw std::invalid_argument("Blueprint parameter names must be unique identifiers");
        }
    }
    std::unordered_set<std::string> tests;
    for(const auto& test:blueprint.tests) {
        if(!identifier(test.name)||!tests.insert(test.name).second)throw std::invalid_argument("Blueprint test names must be unique identifiers");
        validateExpression(test.expression);
    }
    for(const auto& header:blueprint.standardHeaders)
        if(header.empty()||header.find("..")!=std::string::npos||header.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./")!=std::string::npos)
            throw std::invalid_argument("Blueprint standard header is invalid");
}
}

ProjectBlueprint loadProjectBlueprint(const std::filesystem::path& blueprintFile) {
    if(!std::filesystem::is_regular_file(blueprintFile))throw std::invalid_argument("Blueprint file does not exist");
    const auto root=std::filesystem::absolute(blueprintFile).parent_path().lexically_normal();
    std::ifstream stream(blueprintFile,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read project blueprint");
    ProjectBlueprint result;std::string line;std::size_t lineNumber=0;bool version=false;
    while(std::getline(stream,line)) {
        ++lineNumber;if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>65536)throw std::invalid_argument("Blueprint line is too long");
        if(line.empty()||line[0]=='#')continue;
        if(!version) {
            if(line!="GPI_PROJECT_BLUEPRINT_V1")throw std::invalid_argument("Unsupported project blueprint version");
            version=true;continue;
        }
        const auto item=fields(line);const auto malformed=[&]{throw std::invalid_argument("Malformed blueprint line "+std::to_string(lineNumber));};
        if(item[0]=="project") {
            if(item.size()!=2)malformed();
            result.projectName=item[1];
        } else if(item[0]=="namespace") {
            if(item.size()!=2)malformed();
            result.namespaceName=item[1];
        }
        else if(item[0]=="function") {
            if(item.size()!=4)malformed();
            result.functions.push_back({item[1],item[2],item[3],{}});
        } else if(item[0]=="parameter") {
            if(item.size()!=4)malformed();
            const auto found=std::find_if(result.functions.begin(),result.functions.end(),[&](const auto& f){return f.name==item[1];});
            if(found==result.functions.end())throw std::invalid_argument("Blueprint parameter references a function not yet declared");
            found->parameters.push_back({item[2],item[3]});
        } else if(item[0]=="entry") {
            if(item.size()!=3)malformed();
            result.entryType=item[1];result.entryExpression=item[2];
        } else if(item[0]=="test") {
            if(item.size()!=3)malformed();
            result.tests.push_back({item[1],item[2]});
        } else if(item[0]=="include") {
            if(item.size()!=2)malformed();
            result.standardHeaders.push_back(item[1]);
        } else if(item[0]=="input") {
            if(item.size()!=2)malformed();
            result.input=fixture(root,item[1]);
        } else if(item[0]=="expected-output") {
            if(item.size()!=2)malformed();
            result.expectedOutput=fixture(root,item[1]);
        } else if(item[0]=="expected-error") {
            if(item.size()!=2)malformed();
            result.expectedError=fixture(root,item[1]);
        } else if(item[0]=="expected-exit") {
            if(item.size()!=2)malformed();
            result.expectedExitCode=integer(item[1]);
        } else if(item[0]=="timeout-seconds") {
            if(item.size()!=2)malformed();
            const auto seconds=integer(item[1]);if(seconds<1||seconds>60)malformed();
            result.timeout=std::chrono::seconds(seconds);
        } else if(item[0]=="require-sandbox") {
            if(item.size()!=2||(item[1]!="true"&&item[1]!="false"))malformed();
            result.requireSandbox=item[1]=="true";
        } else malformed();
    }
    if(!stream.eof())throw std::runtime_error("Project blueprint read failure");
    if(!version)throw std::invalid_argument("Project blueprint is empty");
    validateBlueprint(result);return result;
}

SynthesizedBlueprintProject synthesizeProjectBlueprint(const ProjectBlueprint& blueprint,Cpp20SourceGenome sourceGenome) {
    validateBlueprint(blueprint);
    if(!sourceGenome.structureEnabled("typed-functions")||
       !sourceGenome.structureEnabled("header-source-executable-test")||
       !sourceGenome.structureEnabled("independent-behavior-tests"))
        throw std::invalid_argument("Immutable genome does not authorize the requested project structure");
    std::set<std::string> headers(blueprint.standardHeaders.begin(),blueprint.standardHeaders.end());
    std::string symbols=blueprint.entryType+blueprint.entryExpression;
    for(const auto& function:blueprint.functions){symbols+=function.returnType+function.expression;for(const auto& parameter:function.parameters)symbols+=parameter.type;}
    for(const auto& test:blueprint.tests)symbols+=test.expression;
    for(const auto& header:sourceGenome.suggestedHeaders(symbols))headers.insert(header);
    std::ostringstream api,implementation,application,test,build;
    api<<"#pragma once\n";for(const auto& header:headers)api<<"#include <"<<header<<">\n";
    api<<"namespace "<<blueprint.namespaceName<<" {\n";
    for(const auto& function:blueprint.functions)api<<function.returnType<<' '<<function.name<<'('<<parameterList(function,false)<<");\n";
    api<<blueprint.entryType<<" gpi_entry_value();\n}\n";
    implementation<<"#include \"generated/api.hpp\"\nnamespace "<<blueprint.namespaceName<<" {\n";
    for(const auto& function:blueprint.functions)
        implementation<<function.returnType<<' '<<function.name<<'('<<parameterList(function,true)<<"){ return ("<<function.expression<<"); }\n";
    implementation<<blueprint.entryType<<" gpi_entry_value(){ return ("<<blueprint.entryExpression<<"); }\n}\n";
    application<<"#include \"generated/api.hpp\"\n#include <iostream>\nint main(){ std::cout << "<<blueprint.namespaceName<<"::gpi_entry_value() << '\\n'; }\n";
    test<<"#include \"generated/api.hpp\"\nint main(){ using namespace "<<blueprint.namespaceName<<"; return (";
    for(std::size_t index=0;index<blueprint.tests.size();++index) {
        if(index)test<<") && (";
        test<<blueprint.tests[index].expression;
    }
    test<<") ? 0 : 1; }\n";
    build<<"cmake_minimum_required(VERSION 3.20)\nproject("<<blueprint.projectName<<" LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\n";
    GenerativeProjectGraph graph(std::move(sourceGenome));
    graph.addNode("api",ProjectNodeKind::Header,"include/generated/api.hpp",api.str());
    graph.addNode("implementation",ProjectNodeKind::Source,"src/generated.cpp",implementation.str());
    graph.addNode("application",ProjectNodeKind::Source,"src/main.cpp",application.str());
    graph.addNode("tests",ProjectNodeKind::Test,"tests/generated_test.cpp",test.str());
    graph.addNode("build",ProjectNodeKind::BuildManifest,"CMakeLists.txt",build.str());
    graph.addDependency("implementation","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","implementation",ProjectDependencyKind::Links);
    graph.addDependency("tests","implementation",ProjectDependencyKind::Tests);
    graph.addDependency("implementation","build",ProjectDependencyKind::Generates);
    graph.addTarget("core",ProjectTargetKind::Library,{"implementation"});
    graph.addTarget("app",ProjectTargetKind::Executable,{"application"});
    graph.addTarget("behavior",ProjectTargetKind::Test,{"tests"});
    ProjectVerificationContract contract;contract.buildTargets={"core","app","behavior"};contract.includeRoots={"include"};
    contract.executionTimeout=blueprint.timeout;contract.requireSandbox=blueprint.requireSandbox;
    contract.behaviors={{"blueprint-output","app",blueprint.input,blueprint.expectedOutput,blueprint.expectedError,blueprint.expectedExitCode},
                        {"blueprint-tests","behavior","","","",0}};
    graph.validate();return {std::move(graph),std::move(contract)};
}
} // namespace genomic::gpi
