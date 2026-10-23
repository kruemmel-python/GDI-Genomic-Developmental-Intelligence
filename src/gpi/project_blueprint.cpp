#include "gpi/project_blueprint.hpp"

#include "cpp20/cpp20_lexicon.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
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
    const auto tokens=Cpp20Lexicon::tokenize(expression);int parentheses=0,brackets=0,braces=0;
    for(const auto& token:tokens) {
        if(token.type==TokenType::Hash||token.type==TokenType::HashHash||
           (token.type>=TokenType::PpDefine&&token.type<=TokenType::PpError))
            throw std::invalid_argument("Blueprint expression contains a statement or preprocessor token");
        if(token.type==TokenType::Semicolon&&braces==0)
            throw std::invalid_argument("Blueprint expression contains a top-level statement separator");
        if(token.type==TokenType::OpenParen)++parentheses;
        if(token.type==TokenType::CloseParen&&--parentheses<0)
            throw std::invalid_argument("Blueprint expression has unbalanced parentheses");
        if(token.type==TokenType::OpenBracket)++brackets;
        if(token.type==TokenType::CloseBracket&&--brackets<0)
            throw std::invalid_argument("Blueprint expression has unbalanced brackets");
        if(token.type==TokenType::OpenBrace)++braces;
        if(token.type==TokenType::CloseBrace&&--braces<0)
            throw std::invalid_argument("Blueprint expression has unbalanced braces");
    }
    if(parentheses||brackets||braces)throw std::invalid_argument("Blueprint expression has unbalanced delimiters");
}
std::filesystem::path safeFixture(const std::filesystem::path& root,const std::string& relative) {
    if(root.empty())throw std::invalid_argument("Blueprint fixture path requires a root directory");
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
long long integer64(const std::string& value) {
    long long result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Blueprint 64-bit integer is invalid");
    return result;
}
std::string parameterList(const std::vector<BlueprintParameter>& parameters,bool names) {
    std::ostringstream out;
    for(std::size_t index=0;index<parameters.size();++index) {
        if(index)out<<", ";
        out<<parameters[index].type;
        if(names)out<<' '<<parameters[index].name;
    }
    return out.str();
}
void validateBlueprint(const ProjectBlueprint& blueprint) {
    if(!identifier(blueprint.projectName)||!qualifiedIdentifier(blueprint.namespaceName)||
       (blueprint.functions.empty()&&blueprint.types.empty())||
       blueprint.entryExpression.empty()||blueprint.tests.empty())
        throw std::invalid_argument("Blueprint lacks required typed project elements");
    validateType(blueprint.entryType);validateExpression(blueprint.entryExpression);

    std::unordered_set<std::string> moduleNames;
    for(const auto& mod:blueprint.modules) {
        if(!identifier(mod.name)||mod.name=="core"||mod.name=="app"||mod.name=="behavior"||!moduleNames.insert(mod.name).second)
            throw std::invalid_argument("Blueprint module names must be unique valid identifiers");
    }
    for(const auto& mod:blueprint.modules) {
        for(const auto& dep:mod.dependencies) {
            if(dep==mod.name||!moduleNames.contains(dep))
                throw std::invalid_argument("Blueprint module dependency references unknown or self module: "+dep);
        }
    }
    // Check module dependency cycle
    std::unordered_map<std::string,int> modState;
    std::function<void(const std::string&)> visitMod=[&](const std::string& m) {
        if(modState[m]==1)throw std::invalid_argument("Blueprint module dependencies contain a cycle");
        if(modState[m]==2)return;
        modState[m]=1;
        for(const auto& mod:blueprint.modules) {
            if(mod.name==m) {
                for(const auto& dep:mod.dependencies)visitMod(dep);
            }
        }
        modState[m]=2;
    };
    for(const auto& mod:blueprint.modules)visitMod(mod.name);

    const auto checkModuleRef=[&](const std::string& mod) {
        if(!mod.empty()&&!moduleNames.contains(mod))
            throw std::invalid_argument("Blueprint element references undeclared module: "+mod);
    };

    std::unordered_set<std::string> typeAndEnumNames;
    for(const auto& en:blueprint.enums) {
        if(!identifier(en.name)||!typeAndEnumNames.insert(en.name).second)
            throw std::invalid_argument("Blueprint enum names must be unique identifiers");
        validateType(en.underlyingType);
        checkModuleRef(en.moduleName);
        if(en.values.empty())throw std::invalid_argument("Blueprint enum must have at least one value: "+en.name);
        std::unordered_set<std::string> valNames;
        for(const auto& val:en.values) {
            if(!identifier(val.name)||!valNames.insert(val.name).second)
                throw std::invalid_argument("Blueprint enum value names must be unique identifiers");
        }
    }

    std::unordered_set<std::string> conceptNames;
    for(const auto& cpt:blueprint.concepts) {
        if(!identifier(cpt.name)||!conceptNames.insert(cpt.name).second||!identifier(cpt.param))
            throw std::invalid_argument("Blueprint concept name and parameter must be valid identifiers");
        validateExpression(cpt.expression);
        checkModuleRef(cpt.moduleName);
    }

    for(const auto& type:blueprint.types) {
        if(!identifier(type.name)||!typeAndEnumNames.insert(type.name).second)
            throw std::invalid_argument("Blueprint type names must be unique identifiers");
        checkModuleRef(type.moduleName);
        if(type.fields.empty())throw std::invalid_argument("Blueprint struct type must have at least one field: "+type.name);
        std::unordered_set<std::string> memberNames;
        for(const auto& field:type.fields) {
            validateType(field.type);
            if(!identifier(field.name)||!memberNames.insert(field.name).second)
                throw std::invalid_argument("Blueprint field names must be unique identifiers");
        }
        for(const auto& method:type.methods) {
            if(!identifier(method.name)||!memberNames.insert(method.name).second)
                throw std::invalid_argument("Blueprint method names must be unique identifiers within type");
            validateType(method.returnType);validateExpression(method.expression);
            std::unordered_set<std::string> params;
            for(const auto& param:method.parameters) {
                validateType(param.type);
                if(!identifier(param.name)||!params.insert(param.name).second)
                    throw std::invalid_argument("Blueprint method parameter names must be unique identifiers");
            }
        }
    }

    std::unordered_set<std::string> names;
    for(const auto& function:blueprint.functions) {
        if(!identifier(function.name)||!names.insert(function.name).second)
            throw std::invalid_argument("Blueprint function names must be unique identifiers");
        validateType(function.returnType);validateExpression(function.expression);
        checkModuleRef(function.moduleName);
        if(!function.templateParam.empty()&&!identifier(function.templateParam))
            throw std::invalid_argument("Blueprint template parameter must be a valid identifier");
        if(!function.requiresConcept.empty()&&!qualifiedIdentifier(function.requiresConcept))
            throw std::invalid_argument("Blueprint requires concept must be a valid identifier");
        std::unordered_set<std::string> parameters;
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

ProjectBlueprint parseProjectBlueprintStream(std::istream& stream,const std::filesystem::path& root) {
    ProjectBlueprint result;std::string line;std::size_t lineNumber=0;bool version=false;
    while(std::getline(stream,line)) {
        ++lineNumber;if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>65536)throw std::invalid_argument("Blueprint line is too long");
        if(line.empty()||line[0]=='#')continue;
        if(!version) {
            if(line!="GPI_PROJECT_BLUEPRINT_V1"&&line!="GPI_PROJECT_BLUEPRINT_V2")
                throw std::invalid_argument("Unsupported project blueprint version");
            version=true;continue;
        }
        const auto item=fields(line);const auto malformed=[&]{throw std::invalid_argument("Malformed blueprint line "+std::to_string(lineNumber));};
        if(item[0]=="project") {
            if(item.size()!=2)malformed();
            result.projectName=item[1];
        } else if(item[0]=="namespace") {
            if(item.size()!=2)malformed();
            result.namespaceName=item[1];
        } else if(item[0]=="module") {
            if(item.size()<2)malformed();
            std::vector<std::string> deps(item.begin()+2,item.end());
            result.modules.push_back({item[1],std::move(deps)});
        } else if(item[0]=="module-dep") {
            if(item.size()!=3)malformed();
            const auto found=std::find_if(result.modules.begin(),result.modules.end(),[&](const auto& m){return m.name==item[1];});
            if(found==result.modules.end())throw std::invalid_argument("Blueprint module-dep references undeclared module");
            found->dependencies.push_back(item[2]);
        } else if(item[0]=="enum") {
            if(item.size()<2||item.size()>4)malformed();
            BlueprintEnum en;en.name=item[1];
            if(item.size()>=3&&!item[2].empty())en.underlyingType=item[2];
            if(item.size()==4)en.moduleName=item[3];
            result.enums.push_back(std::move(en));
        } else if(item[0]=="enum-value") {
            if(item.size()!=4)malformed();
            const auto found=std::find_if(result.enums.begin(),result.enums.end(),[&](const auto& e){return e.name==item[1];});
            if(found==result.enums.end())throw std::invalid_argument("Blueprint enum-value references undeclared enum");
            found->values.push_back({item[2],integer64(item[3])});
        } else if(item[0]=="concept") {
            if(item.size()<4||item.size()>5)malformed();
            result.concepts.push_back({item[1],item[2],item[3],item.size()==5?item[4]:""});
        } else if(item[0]=="type") {
            if(item.size()<2||item.size()>3)malformed();
            result.types.push_back({item[1],item.size()==3?item[2]:"",{},{},true});
        } else if(item[0]=="field") {
            if(item.size()!=4)malformed();
            const auto found=std::find_if(result.types.begin(),result.types.end(),[&](const auto& t){return t.name==item[1];});
            if(found==result.types.end())throw std::invalid_argument("Blueprint field references a type not yet declared");
            found->fields.push_back({item[2],item[3]});
        } else if(item[0]=="method") {
            if(item.size()!=5)malformed();
            const auto found=std::find_if(result.types.begin(),result.types.end(),[&](const auto& t){return t.name==item[1];});
            if(found==result.types.end())throw std::invalid_argument("Blueprint method references a type not yet declared");
            found->methods.push_back({item[2],item[3],item[4],{},true,true});
        } else if(item[0]=="method-parameter") {
            if(item.size()!=5)malformed();
            const auto foundType=std::find_if(result.types.begin(),result.types.end(),[&](const auto& t){return t.name==item[1];});
            if(foundType==result.types.end())throw std::invalid_argument("Blueprint method-parameter references unknown type");
            const auto foundMethod=std::find_if(foundType->methods.begin(),foundType->methods.end(),[&](const auto& m){return m.name==item[2];});
            if(foundMethod==foundType->methods.end())throw std::invalid_argument("Blueprint method-parameter references unknown method");
            foundMethod->parameters.push_back({item[3],item[4]});
        } else if(item[0]=="function") {
            if(item.size()<4||item.size()>7)malformed();
            BlueprintFunction fn{item[1],item[2],item[3],{},item.size()>=5?item[4]:"",item.size()>=6?item[5]:"",item.size()>=7?item[6]:"",false};
            result.functions.push_back(std::move(fn));
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
        } else if(item[0]=="expected-output-literal") {
            if(item.size()!=2)malformed();
            result.expectedOutput=item[1]+"\n";
        } else malformed();
    }
    if(stream.bad())throw std::runtime_error("Project blueprint read failure");
    if(!version)throw std::invalid_argument("Project blueprint is empty");
    validateBlueprint(result);return result;
}
}

ProjectBlueprint loadProjectBlueprint(const std::filesystem::path& blueprintFile) {
    if(!std::filesystem::is_regular_file(blueprintFile))throw std::invalid_argument("Blueprint file does not exist");
    const auto root=std::filesystem::absolute(blueprintFile).parent_path().lexically_normal();
    std::ifstream stream(blueprintFile,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read project blueprint");
    return parseProjectBlueprintStream(stream,root);
}

ProjectBlueprint parseProjectBlueprintText(const std::string& text,const std::filesystem::path& fixtureRoot) {
    std::istringstream stream(text);
    return parseProjectBlueprintStream(stream,fixtureRoot);
}

std::string serializeProjectBlueprint(const ProjectBlueprint& blueprint) {
    std::ostringstream out;
    const bool v2=!blueprint.modules.empty()||!blueprint.types.empty()||!blueprint.enums.empty()||!blueprint.concepts.empty();
    out<<(v2?"GPI_PROJECT_BLUEPRINT_V2":"GPI_PROJECT_BLUEPRINT_V1")<<"\nproject\t"<<blueprint.projectName
       <<"\nnamespace\t"<<blueprint.namespaceName<<'\n';
    for(const auto& mod:blueprint.modules) {
        out<<"module\t"<<mod.name;
        for(const auto& dep:mod.dependencies)out<<'\t'<<dep;
        out<<'\n';
    }
    for(const auto& en:blueprint.enums) {
        out<<"enum\t"<<en.name<<'\t'<<en.underlyingType;
        if(!en.moduleName.empty())out<<'\t'<<en.moduleName;
        out<<'\n';
        for(const auto& val:en.values)out<<"enum-value\t"<<en.name<<'\t'<<val.name<<'\t'<<val.value<<'\n';
    }
    for(const auto& cpt:blueprint.concepts) {
        out<<"concept\t"<<cpt.name<<'\t'<<cpt.param<<'\t'<<cpt.expression;
        if(!cpt.moduleName.empty())out<<'\t'<<cpt.moduleName;
        out<<'\n';
    }
    for(const auto& type:blueprint.types) {
        out<<"type\t"<<type.name;
        if(!type.moduleName.empty())out<<'\t'<<type.moduleName;
        out<<'\n';
        for(const auto& field:type.fields)out<<"field\t"<<type.name<<'\t'<<field.type<<'\t'<<field.name<<'\n';
        for(const auto& method:type.methods) {
            out<<"method\t"<<type.name<<'\t'<<method.name<<'\t'<<method.returnType<<'\t'<<method.expression<<'\n';
            for(const auto& param:method.parameters)
                out<<"method-parameter\t"<<type.name<<'\t'<<method.name<<'\t'<<param.type<<'\t'<<param.name<<'\n';
        }
    }
    for(const auto& fn:blueprint.functions) {
        out<<"function\t"<<fn.name<<'\t'<<fn.returnType<<'\t'<<fn.expression;
        if(!fn.moduleName.empty()||!fn.templateParam.empty()||!fn.requiresConcept.empty()) {
            out<<'\t'<<fn.moduleName;
            if(!fn.templateParam.empty()||!fn.requiresConcept.empty()) {
                out<<'\t'<<fn.templateParam;
                if(!fn.requiresConcept.empty())out<<'\t'<<fn.requiresConcept;
            }
        }
        out<<'\n';
        for(const auto& param:fn.parameters)out<<"parameter\t"<<fn.name<<'\t'<<param.type<<'\t'<<param.name<<'\n';
    }
    out<<"entry\t"<<blueprint.entryType<<'\t'<<blueprint.entryExpression<<'\n';
    for(const auto& test:blueprint.tests)out<<"test\t"<<test.name<<'\t'<<test.expression<<'\n';
    for(const auto& header:blueprint.standardHeaders)out<<"include\t"<<header<<'\n';
    if(!blueprint.expectedOutput.empty()) {
        std::string trimmed=blueprint.expectedOutput;
        if(!trimmed.empty()&&trimmed.back()=='\n')trimmed.pop_back();
        if(trimmed.find('\n')==std::string::npos)out<<"expected-output-literal\t"<<trimmed<<'\n';
    }
    out<<"timeout-seconds\t"<<blueprint.timeout.count()<<"\nrequire-sandbox\t"<<(blueprint.requireSandbox?"true":"false")<<'\n';
    return out.str();
}

SynthesizedBlueprintProject synthesizeProjectBlueprint(const ProjectBlueprint& blueprint,Cpp20SourceGenome sourceGenome) {
    validateBlueprint(blueprint);
    if(!sourceGenome.structureEnabled("typed-functions")||
       !sourceGenome.structureEnabled("header-source-executable-test")||
       !sourceGenome.structureEnabled("independent-behavior-tests"))
        throw std::invalid_argument("Immutable genome does not authorize the requested project structure");
    if(!blueprint.types.empty()&&!sourceGenome.structureEnabled("user-defined-types"))
        throw std::invalid_argument("Immutable genome does not authorize user-defined types");
    if(!blueprint.enums.empty()&&!sourceGenome.structureEnabled("enum-types"))
        throw std::invalid_argument("Immutable genome does not authorize enum types");
    if(!blueprint.modules.empty()&&!sourceGenome.structureEnabled("multi-module-components"))
        throw std::invalid_argument("Immutable genome does not authorize multi-module components");
    if(!blueprint.concepts.empty()&&!sourceGenome.structureEnabled("cpp20-concepts"))
        throw std::invalid_argument("Immutable genome does not authorize C++20 concepts");

    std::set<std::string> headers(blueprint.standardHeaders.begin(),blueprint.standardHeaders.end());
    if(!blueprint.concepts.empty())headers.insert("concepts");
    std::string symbols=blueprint.entryType+blueprint.entryExpression;
    for(const auto& cpt:blueprint.concepts)symbols+=cpt.expression;
    for(const auto& type:blueprint.types) {
        for(const auto& field:type.fields)symbols+=field.type;
        for(const auto& method:type.methods) {
            if(!sourceGenome.structureEnabled("type-methods"))
                throw std::invalid_argument("Immutable genome does not authorize type methods");
            symbols+=method.returnType+method.expression;
            for(const auto& param:method.parameters)symbols+=param.type;
        }
    }
    for(const auto& function:blueprint.functions) {
        if(!function.templateParam.empty()&&!sourceGenome.structureEnabled("cpp20-templates"))
            throw std::invalid_argument("Immutable genome does not authorize C++20 templates");
        symbols+=function.returnType+function.expression+function.requiresConcept;
        for(const auto& parameter:function.parameters)symbols+=parameter.type;
    }
    for(const auto& test:blueprint.tests)symbols+=test.expression;
    for(const auto& header:sourceGenome.suggestedHeaders(symbols))headers.insert(header);

    const auto emitModuleTypesAndFunctions=[&](const std::string& modName,std::ostringstream& hdr,std::ostringstream& src) {
        for(const auto& en:blueprint.enums) {
            if(en.moduleName!=modName)continue;
            hdr<<"enum class "<<en.name<<" : "<<en.underlyingType<<" {\n";
            for(std::size_t i=0;i<en.values.size();++i) {
                hdr<<"    "<<en.values[i].name<<" = "<<en.values[i].value<<(i+1<en.values.size()?",\n":"\n");
            }
            hdr<<"};\n";
        }
        for(const auto& cpt:blueprint.concepts) {
            if(cpt.moduleName!=modName)continue;
            hdr<<"template<typename "<<cpt.param<<">\nconcept "<<cpt.name<<" = "<<cpt.expression<<";\n";
        }
        for(const auto& type:blueprint.types) {
            if(type.moduleName!=modName)continue;
            hdr<<"struct "<<type.name<<" {\n";
            for(const auto& field:type.fields)hdr<<"    "<<field.type<<' '<<field.name<<"{};\n";
            for(const auto& method:type.methods) {
                hdr<<"    "<<(method.isConstexpr?"constexpr ":"")<<method.returnType<<' '<<method.name
                   <<'('<<parameterList(method.parameters,true)<<')'<<(method.isConst?" const":"")
                   <<" { return ("<<method.expression<<"); }\n";
            }
            if(type.generateEquality) {
                hdr<<"    constexpr bool operator==(const "<<type.name<<"&) const = default;\n";
            }
            hdr<<"};\n";
        }
        for(const auto& function:blueprint.functions) {
            if(function.moduleName!=modName)continue;
            if(!function.templateParam.empty()) {
                hdr<<"template<typename "<<function.templateParam<<">\n";
                if(!function.requiresConcept.empty())
                    hdr<<"requires "<<function.requiresConcept<<"<"<<function.templateParam<<">\n";
                hdr<<"constexpr "<<function.returnType<<' '<<function.name<<'('<<parameterList(function.parameters,true)<<") { return ("<<function.expression<<"); }\n";
            } else if(function.isConstexpr) {
                hdr<<"constexpr "<<function.returnType<<' '<<function.name<<'('<<parameterList(function.parameters,true)<<") { return ("<<function.expression<<"); }\n";
            } else {
                hdr<<function.returnType<<' '<<function.name<<'('<<parameterList(function.parameters,false)<<");\n";
                src<<function.returnType<<' '<<function.name<<'('<<parameterList(function.parameters,true)<<"){ return ("<<function.expression<<"); }\n";
            }
        }
    };

    GenerativeProjectGraph graph(std::move(sourceGenome));
    ProjectVerificationContract contract;
    contract.includeRoots={"include"};
    contract.executionTimeout=blueprint.timeout;
    contract.requireSandbox=blueprint.requireSandbox;

    // Emit domain modules if any
    for(const auto& mod:blueprint.modules) {
        std::ostringstream modHdr,modSrc;
        modHdr<<"#pragma once\n";
        for(const auto& header:headers)modHdr<<"#include <"<<header<<">\n";
        for(const auto& dep:mod.dependencies)modHdr<<"#include \"generated/"<<dep<<".hpp\"\n";
        modHdr<<"namespace "<<blueprint.namespaceName<<" {\n";
        modSrc<<"#include \"generated/"<<mod.name<<".hpp\"\nnamespace "<<blueprint.namespaceName<<" {\n";
        emitModuleTypesAndFunctions(mod.name,modHdr,modSrc);
        // Ensure every module source has at least one translation-unit symbol even if header-only types
        modSrc<<"int gpi_module_anchor_"<<mod.name<<"() { return 0; }\n}\n";
        modHdr<<"int gpi_module_anchor_"<<mod.name<<"();\n}\n";

        const auto hdrId="api_"+mod.name;
        const auto srcId="impl_"+mod.name;
        graph.addNode(hdrId,ProjectNodeKind::Header,"include/generated/"+mod.name+".hpp",modHdr.str());
        graph.addNode(srcId,ProjectNodeKind::Source,"src/"+mod.name+".cpp",modSrc.str());
        graph.addDependency(srcId,hdrId,ProjectDependencyKind::Includes);
        for(const auto& dep:mod.dependencies) {
            graph.addDependency(hdrId,"api_"+dep,ProjectDependencyKind::Includes);
            graph.addDependency(srcId,"impl_"+dep,ProjectDependencyKind::Links);
        }
        graph.addTarget("mod_"+mod.name,ProjectTargetKind::Library,{srcId});
        contract.buildTargets.push_back("mod_"+mod.name);
    }

    std::ostringstream api,implementation,application,test,build;
    api<<"#pragma once\n";
    for(const auto& header:headers)api<<"#include <"<<header<<">\n";
    for(const auto& mod:blueprint.modules)api<<"#include \"generated/"<<mod.name<<".hpp\"\n";
    api<<"namespace "<<blueprint.namespaceName<<" {\n";
    implementation<<"#include \"generated/api.hpp\"\nnamespace "<<blueprint.namespaceName<<" {\n";
    emitModuleTypesAndFunctions("",api,implementation);
    api<<blueprint.entryType<<" gpi_entry_value();\n}\n";
    implementation<<blueprint.entryType<<" gpi_entry_value(){ return ("<<blueprint.entryExpression<<"); }\n}\n";

    application<<"#include \"generated/api.hpp\"\n#include <iostream>\nint main(){ std::cout << "<<blueprint.namespaceName<<"::gpi_entry_value() << '\\n'; }\n";
    test<<"#include \"generated/api.hpp\"\nint main(){ using namespace "<<blueprint.namespaceName<<"; return (";
    for(std::size_t index=0;index<blueprint.tests.size();++index) {
        if(index)test<<") && (";
        test<<blueprint.tests[index].expression;
    }
    test<<") ? 0 : 1; }\n";
    build<<"cmake_minimum_required(VERSION 3.20)\nproject("<<blueprint.projectName<<" LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\n";

    graph.addNode("api",ProjectNodeKind::Header,"include/generated/api.hpp",api.str());
    graph.addNode("implementation",ProjectNodeKind::Source,"src/generated.cpp",implementation.str());
    graph.addNode("application",ProjectNodeKind::Source,"src/main.cpp",application.str());
    graph.addNode("tests",ProjectNodeKind::Test,"tests/generated_test.cpp",test.str());
    graph.addNode("build",ProjectNodeKind::BuildManifest,"CMakeLists.txt",build.str());

    for(const auto& mod:blueprint.modules) {
        graph.addDependency("api","api_"+mod.name,ProjectDependencyKind::Includes);
        graph.addDependency("implementation","impl_"+mod.name,ProjectDependencyKind::Links);
    }
    graph.addDependency("implementation","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","implementation",ProjectDependencyKind::Links);
    graph.addDependency("tests","implementation",ProjectDependencyKind::Tests);
    graph.addDependency("implementation","build",ProjectDependencyKind::Generates);

    graph.addTarget("core",ProjectTargetKind::Library,{"implementation"});
    graph.addTarget("app",ProjectTargetKind::Executable,{"application"});
    graph.addTarget("behavior",ProjectTargetKind::Test,{"tests"});
    contract.buildTargets.push_back("core");
    contract.buildTargets.push_back("app");
    contract.buildTargets.push_back("behavior");
    contract.behaviors={{"blueprint-output","app",blueprint.input,blueprint.expectedOutput,blueprint.expectedError,blueprint.expectedExitCode},
                        {"blueprint-tests","behavior","","","",0}};
    graph.validate();return {std::move(graph),std::move(contract)};
}
} // namespace genomic::gpi
