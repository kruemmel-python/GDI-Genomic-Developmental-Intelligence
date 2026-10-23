#include "gpi/goal_architect.hpp"

#include "cpp20/cpp20_lexicon.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace genomic::gpi {
namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> result;std::size_t begin=0;
    while(begin<=line.size()) {
        const auto end=line.find('\t',begin);result.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return result;
}
bool identifier(const std::string& value) {
    if(value.empty()||Cpp20Lexicon::isKeyword(value)||
       !(std::isalpha(static_cast<unsigned char>(value.front()))||value.front()=='_'))return false;
    return std::all_of(value.begin()+1,value.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}
bool qualifiedIdentifier(const std::string& value) {
    std::size_t begin=0;
    while(begin<value.size()) {
        const auto end=value.find("::",begin);if(!identifier(value.substr(begin,end==std::string::npos?end:end-begin)))return false;
        if(end==std::string::npos)return true;
        begin=end+2;
    }
    return false;
}
bool safeType(const std::string& type) {
    return !type.empty()&&type.find_first_of(";{}#()=[]\r\n\t") == std::string::npos;
}
bool integerLiteral(const std::string& value) {
    if(value.size()<2||value.front()!='#')return false;
    std::int64_t parsed=0;
    const auto first=value.data()+1;const auto [end,error]=std::from_chars(first,value.data()+value.size(),parsed);
    return error==std::errc{}&&end==value.data()+value.size();
}
bool boolLiteral(const std::string& value) {return value=="true"||value=="false";}
std::string literalSource(const std::string& value) {return integerLiteral(value)?value.substr(1):value;}
bool booleanType(const std::string& type) {return type=="bool";}
bool numericType(const std::string& type) {
    return type=="int"||type=="long"||type=="long long"||type=="int32_t"||type=="int64_t"||
           type=="uint32_t"||type=="uint64_t"||type=="double"||type=="float"||type=="literal";
}
bool compatible(const std::string& left,const std::string& right) {
    if(left==right)return true;
    if(left=="literal"||right=="literal")return numericType(left)&&numericType(right);
    if(booleanType(left)||booleanType(right))return booleanType(left)&&booleanType(right);
    if(numericType(left)&&numericType(right))return true;
    return false;
}
int parseInt(const std::string& value) {
    int result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Goal integer is invalid");
    return result;
}
long long parseInt64(const std::string& value) {
    long long result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Goal 64-bit integer is invalid");
    return result;
}
std::string json(const std::string& value) {
    std::ostringstream out;out<<'"';for(const unsigned char c:value) {
        if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
        else if(c=='\n')out<<"\\n";else if(c=='\r')out<<"\\r";else if(c=='\t')out<<"\\t";
        else out<<static_cast<char>(c);
    }return out.str()+'"';
}
void hashField(std::uint64_t& hash,const std::string& value) {
    const auto framed=std::to_string(value.size())+":"+value;
    for(const unsigned char byte:framed){hash^=byte;hash*=1099511628211ULL;}
}
std::uint64_t semanticHash(const TypedArchitectureGoal& goal) {
    std::uint64_t hash=1469598103934665603ULL;hashField(hash,goal.projectName);hashField(hash,goal.namespaceName);
    for(const auto& mod:goal.modules) {
        hashField(hash,mod.name);
        for(const auto& dep:mod.dependencies)hashField(hash,dep);
    }
    for(const auto& en:goal.enums) {
        hashField(hash,en.name);hashField(hash,en.underlyingType);hashField(hash,en.moduleName);
        for(const auto& val:en.values){hashField(hash,val.name);hashField(hash,std::to_string(val.value));}
    }
    for(const auto& cpt:goal.concepts) {
        hashField(hash,cpt.name);hashField(hash,cpt.param);hashField(hash,cpt.expression);hashField(hash,cpt.moduleName);
    }
    for(const auto& type:goal.types) {
        hashField(hash,type.name);hashField(hash,type.moduleName);
        for(const auto& field:type.fields){hashField(hash,field.type);hashField(hash,field.name);}
        for(const auto& method:type.methods) {
            hashField(hash,method.name);hashField(hash,method.returnType);
            for(const auto& param:method.parameters){hashField(hash,param.type);hashField(hash,param.name);}
            for(const auto& op:method.operations) {
                hashField(hash,op.id);hashField(hash,op.operation);
                for(const auto& operand:op.operands)hashField(hash,operand);
            }
            hashField(hash,method.returnValue);
        }
    }
    for(const auto& function:goal.functions) {
        hashField(hash,function.name);hashField(hash,function.returnType);
        if(!function.moduleName.empty())hashField(hash,function.moduleName);
        if(!function.templateParam.empty())hashField(hash,function.templateParam);
        if(!function.requiresConcept.empty())hashField(hash,function.requiresConcept);
        for(const auto& parameter:function.parameters){hashField(hash,parameter.type);hashField(hash,parameter.name);}
        for(const auto& operation:function.operations) {
            hashField(hash,operation.id);hashField(hash,operation.operation);
            for(const auto& operand:operation.operands)hashField(hash,operand);
        }
        hashField(hash,function.returnValue);
    }
    hashField(hash,goal.entryFunction);hashField(hash,goal.entryExpected);
    for(const auto& argument:goal.entryArguments)hashField(hash,argument);
    for(const auto& item:goal.cases) {
        hashField(hash,item.name);hashField(hash,item.function);hashField(hash,item.expected);
        for(const auto& argument:item.arguments)hashField(hash,argument);
    }
    hashField(hash,std::to_string(goal.timeout.count()));hashField(hash,goal.requireSandbox?"true":"false");return hash;
}
const GoalFunction& functionNamed(const TypedArchitectureGoal& goal,const std::string& name) {
    const auto found=std::find_if(goal.functions.begin(),goal.functions.end(),[&](const auto& function){return function.name==name;});
    if(found==goal.functions.end())throw std::invalid_argument("Goal references unknown function: "+name);
    return *found;
}
const GoalType* typeNamedOpt(const TypedArchitectureGoal& goal,const std::string& name) {
    const auto found=std::find_if(goal.types.begin(),goal.types.end(),[&](const auto& t){return t.name==name;});
    return found==goal.types.end()?nullptr:&*found;
}
const GoalType& typeNamed(const TypedArchitectureGoal& goal,const std::string& name) {
    const auto* found=typeNamedOpt(goal,name);
    if(!found)throw std::invalid_argument("Goal references unknown user type: "+name);
    return *found;
}
GoalMethod& methodNamed(GoalType& type,const std::string& name) {
    const auto found=std::find_if(type.methods.begin(),type.methods.end(),[&](const auto& m){return m.name==name;});
    if(found==type.methods.end())throw std::invalid_argument("Goal references unknown method "+name+" on type "+type.name);
    return *found;
}
const GoalEnum* enumNamedOpt(const TypedArchitectureGoal& goal,const std::string& name) {
    const auto found=std::find_if(goal.enums.begin(),goal.enums.end(),[&](const auto& e){return e.name==name;});
    return found==goal.enums.end()?nullptr:&*found;
}

// Parse a case argument or expectation:
// - `#123` -> integer literal
// - `true` / `false` -> bool literal
// - `TypeName{arg1,arg2}` -> aggregate literal of user-defined type `TypeName`
// - `EnumName::ValueName` -> enum value literal
bool parseCaseLiteral(const TypedArchitectureGoal& goal,const std::string& expectedType,const std::string& raw,std::string& cppOut) {
    if(integerLiteral(raw)) {
        if(!numericType(expectedType))return false;
        cppOut=literalSource(raw);
        return true;
    }
    if(boolLiteral(raw)) {
        if(!booleanType(expectedType))return false;
        cppOut=raw;
        return true;
    }
    const auto colon=raw.find("::");
    if(colon!=std::string::npos) {
        const auto enumName=raw.substr(0,colon);
        const auto valName=raw.substr(colon+2);
        const auto* en=enumNamedOpt(goal,enumName);
        if(!en||en->name!=expectedType)return false;
        const auto found=std::find_if(en->values.begin(),en->values.end(),[&](const auto& v){return v.name==valName;});
        if(found==en->values.end())return false;
        cppOut=enumName+"::"+valName;
        return true;
    }
    const auto openBrace=raw.find('{');
    if(openBrace!=std::string::npos&&raw.back()=='}') {
        const auto tName=raw.substr(0,openBrace);
        const auto* uType=typeNamedOpt(goal,tName);
        if(!uType||uType->name!=expectedType)return false;
        const auto inner=raw.substr(openBrace+1,raw.size()-openBrace-2);
        std::vector<std::string> parts;
        std::size_t start=0;int depth=0;
        for(std::size_t i=0;i<=inner.size();++i) {
            if(i<inner.size()&&inner[i]=='{')++depth;
            else if(i<inner.size()&&inner[i]=='}')--depth;
            else if((i==inner.size()||inner[i]==',')&&depth==0) {
                parts.push_back(inner.substr(start,i-start));
                start=i+1;
            }
        }
        if(parts.size()!=uType->fields.size())return false;
        std::ostringstream out;
        out<<tName<<'{';
        for(std::size_t i=0;i<parts.size();++i) {
            std::string subCpp;
            if(!parseCaseLiteral(goal,uType->fields[i].type,parts[i],subCpp))return false;
            if(i)out<<", ";
            out<<subCpp;
        }
        out<<'}';
        cppOut=out.str();
        return true;
    }
    return false;
}

std::string callExpression(const TypedArchitectureGoal& goal,const GoalFunction& function,const std::vector<std::string>& arguments) {
    if(arguments.size()!=function.parameters.size())throw std::invalid_argument("Goal call has wrong argument count: "+function.name);
    std::ostringstream out;out<<function.name<<'(';for(std::size_t i=0;i<arguments.size();++i) {
        std::string argSource;
        const std::string paramType=(!function.templateParam.empty()&&function.parameters[i].type==function.templateParam)
            ?"long long":function.parameters[i].type;
        if(!parseCaseLiteral(goal,paramType,arguments[i],argSource))
            throw std::invalid_argument("Goal case literal type mismatch or invalid literal: "+arguments[i]);
        if(i)out<<", ";
        out<<argSource;
    }return out.str()+')';
}

void validateShape(const TypedArchitectureGoal& goal) {
    if(!identifier(goal.projectName)||!qualifiedIdentifier(goal.namespaceName)||goal.functions.empty()||
       goal.entryFunction.empty()||goal.cases.empty())throw std::invalid_argument("Goal lacks required architecture elements");
    std::unordered_set<std::string> moduleNames;
    for(const auto& mod:goal.modules) {
        if(!identifier(mod.name)||!moduleNames.insert(mod.name).second)
            throw std::invalid_argument("Goal module names must be unique identifiers");
    }
    std::unordered_set<std::string> typeNames;
    for(const auto& en:goal.enums) {
        if(!identifier(en.name)||!typeNames.insert(en.name).second||en.values.empty())
            throw std::invalid_argument("Goal enum is invalid or duplicated: "+en.name);
    }
    for(const auto& type:goal.types) {
        if(!identifier(type.name)||!typeNames.insert(type.name).second||type.fields.empty())
            throw std::invalid_argument("Goal user-defined type is invalid or duplicated: "+type.name);
        std::unordered_set<std::string> members;
        for(const auto& f:type.fields) {
            if(!safeType(f.type)||!identifier(f.name)||!members.insert(f.name).second)
                throw std::invalid_argument("Goal type field is invalid or duplicated: "+f.name);
        }
        for(const auto& m:type.methods) {
            if(!identifier(m.name)||!members.insert(m.name).second||!safeType(m.returnType)||
               m.operations.empty()||m.returnValue.empty())
                throw std::invalid_argument("Goal type method is incomplete or duplicated: "+m.name);
        }
    }
    std::unordered_set<std::string> functions;
    for(const auto& function:goal.functions) {
        if(!identifier(function.name)||!functions.insert(function.name).second||!safeType(function.returnType)||
           function.operations.empty()||function.returnValue.empty())throw std::invalid_argument("Goal function is incomplete or duplicated");
        std::unordered_set<std::string> names;
        for(const auto& parameter:function.parameters)
            if(!safeType(parameter.type)||!identifier(parameter.name)||!names.insert(parameter.name).second)
                throw std::invalid_argument("Goal parameter is unsafe or duplicated");
        for(const auto& operation:function.operations)
            if(!identifier(operation.id)||!names.insert(operation.id).second||operation.operation.empty())
                throw std::invalid_argument("Goal operation is unsafe or duplicated");
    }
    std::unordered_set<std::string> cases;
    for(const auto& item:goal.cases)
        if(!identifier(item.name)||!cases.insert(item.name).second)throw std::invalid_argument("Goal case names must be unique identifiers");
}

TypedArchitectureGoal parseTypedArchitectureGoalStream(std::istream& stream) {
    TypedArchitectureGoal result;std::string line;std::size_t lineNumber=0;bool version=false;
    while(std::getline(stream,line)) {
        ++lineNumber;if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>65536)throw std::invalid_argument("Goal line is too long");
        if(line.empty()||line.front()=='#')continue;
        if(!version){
            if(line!="GPI_TYPED_ARCHITECTURE_GOAL_V1"&&line!="GPI_TYPED_ARCHITECTURE_GOAL_V2")
                throw std::invalid_argument("Unsupported architecture goal version");
            version=true;continue;
        }
        const auto item=split(line);const auto malformed=[&]{throw std::invalid_argument("Malformed goal line "+std::to_string(lineNumber));};
        if(item[0]=="project") {if(item.size()!=2)malformed();result.projectName=item[1];}
        else if(item[0]=="namespace") {if(item.size()!=2)malformed();result.namespaceName=item[1];}
        else if(item[0]=="module") {
            if(item.size()<2)malformed();
            result.modules.push_back({item[1],{item.begin()+2,item.end()}});
        } else if(item[0]=="module-dep") {
            if(item.size()!=3)malformed();
            const auto found=std::find_if(result.modules.begin(),result.modules.end(),[&](const auto& m){return m.name==item[1];});
            if(found==result.modules.end())throw std::invalid_argument("Goal module-dep references undeclared module");
            found->dependencies.push_back(item[2]);
        } else if(item[0]=="enum") {
            if(item.size()<2||item.size()>4)malformed();
            GoalEnum en;en.name=item[1];
            if(item.size()>=3&&!item[2].empty())en.underlyingType=item[2];
            if(item.size()==4)en.moduleName=item[3];
            result.enums.push_back(std::move(en));
        } else if(item[0]=="enum-value") {
            if(item.size()!=4)malformed();
            const auto found=std::find_if(result.enums.begin(),result.enums.end(),[&](const auto& e){return e.name==item[1];});
            if(found==result.enums.end())throw std::invalid_argument("Goal enum-value references undeclared enum");
            found->values.push_back({item[2],parseInt64(literalSource(item[3]))});
        } else if(item[0]=="concept") {
            if(item.size()<4||item.size()>5)malformed();
            result.concepts.push_back({item[1],item[2],item[3],item.size()==5?item[4]:""});
        } else if(item[0]=="type") {
            if(item.size()<2||item.size()>3)malformed();
            result.types.push_back({item[1],item.size()==3?item[2]:"",{},{}});
        } else if(item[0]=="field") {
            if(item.size()!=4)malformed();
            auto& type=const_cast<GoalType&>(typeNamed(result,item[1]));
            type.fields.push_back({item[2],item[3]});
        } else if(item[0]=="method") {
            if(item.size()!=4)malformed();
            auto& type=const_cast<GoalType&>(typeNamed(result,item[1]));
            type.methods.push_back({item[2],item[3],{},{},{}});
        } else if(item[0]=="method-parameter") {
            if(item.size()!=5)malformed();
            auto& type=const_cast<GoalType&>(typeNamed(result,item[1]));
            auto& method=methodNamed(type,item[2]);
            method.parameters.push_back({item[3],item[4]});
        } else if(item[0]=="method-operation") {
            if(item.size()<6)malformed();
            auto& type=const_cast<GoalType&>(typeNamed(result,item[1]));
            auto& method=methodNamed(type,item[2]);
            method.operations.push_back({item[3],item[4],{item.begin()+5,item.end()}});
        } else if(item[0]=="method-return") {
            if(item.size()!=4)malformed();
            auto& type=const_cast<GoalType&>(typeNamed(result,item[1]));
            auto& method=methodNamed(type,item[2]);
            method.returnValue=item[3];
        } else if(item[0]=="function") {
            if(item.size()<3||item.size()>6)malformed();
            GoalFunction fn{item[1],item[2],{},{},{},item.size()>=4?item[3]:"",item.size()>=5?item[4]:"",item.size()>=6?item[5]:"",false};
            result.functions.push_back(std::move(fn));
        } else if(item[0]=="parameter") {
            if(item.size()!=4)malformed();
            auto& function=const_cast<GoalFunction&>(functionNamed(result,item[1]));
            function.parameters.push_back({item[2],item[3]});
        } else if(item[0]=="operation") {
            if(item.size()<5)malformed();
            auto& function=const_cast<GoalFunction&>(functionNamed(result,item[1]));
            function.operations.push_back({item[2],item[3],{item.begin()+4,item.end()}});
        } else if(item[0]=="return") {
            if(item.size()!=3)malformed();
            auto& function=const_cast<GoalFunction&>(functionNamed(result,item[1]));function.returnValue=item[2];
        } else if(item[0]=="entry") {
            if(item.size()<3)malformed();
            result.entryFunction=item[1];result.entryExpected=item[2];
            result.entryArguments.assign(item.begin()+3,item.end());
        } else if(item[0]=="case") {
            if(item.size()<4)malformed();
            result.cases.push_back({item[1],item[2],item[3],{item.begin()+4,item.end()}});
        } else if(item[0]=="timeout-seconds") {
            if(item.size()!=2)malformed();
            const auto seconds=parseInt(item[1]);if(seconds<1||seconds>60)malformed();result.timeout=std::chrono::seconds(seconds);
        } else if(item[0]=="require-sandbox") {
            if(item.size()!=2||(item[1]!="true"&&item[1]!="false"))malformed();
            result.requireSandbox=item[1]=="true";
        } else malformed();
    }
    if(stream.bad())throw std::runtime_error("Typed architecture goal read failure");
    if(!version)throw std::invalid_argument("Typed architecture goal is empty");
    validateShape(result);return result;
}
}

TypedArchitectureGoal loadTypedArchitectureGoal(const std::filesystem::path& goalFile) {
    if(!std::filesystem::is_regular_file(goalFile))throw std::invalid_argument("Goal file does not exist");
    std::ifstream stream(goalFile,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read typed architecture goal");
    return parseTypedArchitectureGoalStream(stream);
}

TypedArchitectureGoal parseTypedArchitectureGoalText(const std::string& text) {
    std::istringstream stream(text);
    return parseTypedArchitectureGoalStream(stream);
}

ArchitectedProject architectTypedGoal(const TypedArchitectureGoal& goal,Cpp20SourceGenome sourceGenome) {
    validateShape(goal);
    if(!sourceGenome.structureEnabled("typed-functions")||!sourceGenome.structureEnabled("header-source-executable-test")||
       !sourceGenome.structureEnabled("independent-behavior-tests"))
        throw std::invalid_argument("Immutable genome does not authorize typed project architecture");
    ProjectBlueprint blueprint;blueprint.projectName=goal.projectName;blueprint.namespaceName=goal.namespaceName;
    blueprint.timeout=goal.timeout;blueprint.requireSandbox=goal.requireSandbox;
    for(const auto& mod:goal.modules)blueprint.modules.push_back({mod.name,mod.dependencies});
    for(const auto& en:goal.enums) {
        BlueprintEnum ben{en.name,en.underlyingType,en.moduleName,{}};
        for(const auto& v:en.values)ben.values.push_back({v.name,v.value});
        blueprint.enums.push_back(std::move(ben));
    }
    for(const auto& cpt:goal.concepts)blueprint.concepts.push_back({cpt.name,cpt.param,cpt.expression,cpt.moduleName});

    const auto goalHash=semanticHash(goal);

    // Helper to resolve an operation graph (used for both free functions and struct methods)
    const auto resolveGraph=[&](const std::string& ownerName,const std::string& returnType,
                                const std::vector<GoalParameter>& parameters,
                                const std::vector<GoalField>& implicitFields,
                                const std::vector<GoalOperation>& ops,
                                const std::string& returnVal,
                                std::vector<std::string>& dependencies)->std::string {
        std::unordered_map<std::string,std::string> types,expressions;
        std::unordered_map<std::string,const GoalOperation*> operations;
        for(const auto& f:implicitFields) {
            types.emplace(f.name,f.type);
            expressions.emplace(f.name,"this->"+f.name);
        }
        for(const auto& parameter:parameters) {
            types.emplace(parameter.name,parameter.type);
            expressions.emplace(parameter.name,parameter.name);
        }
        for(const auto& operation:ops)operations.emplace(operation.id,&operation);
        std::unordered_set<std::string> active,done;

        std::function<void(const std::string&)> resolve=[&](const std::string& id) {
            if(done.contains(id)||types.contains(id))return;
            const auto found=operations.find(id);if(found==operations.end())throw std::invalid_argument("Goal references unknown value: "+id);
            if(!active.insert(id).second)throw std::invalid_argument("Goal operation graph contains a cycle");
            const auto& node=*found->second;std::vector<std::string> args,argTypes;
            for(const auto& operand:node.operands) {
                if(integerLiteral(operand)){args.push_back(literalSource(operand));argTypes.push_back("literal");continue;}
                if(boolLiteral(operand)){args.push_back(operand);argTypes.push_back("bool");continue;}
                const auto colon=operand.find("::");
                if(colon!=std::string::npos) {
                    const auto enumName=operand.substr(0,colon);
                    const auto valName=operand.substr(colon+2);
                    const auto* en=enumNamedOpt(goal,enumName);
                    if(!en)throw std::invalid_argument("Unknown enum in operand: "+operand);
                    const auto vIt=std::find_if(en->values.begin(),en->values.end(),[&](const auto& v){return v.name==valName;});
                    if(vIt==en->values.end())throw std::invalid_argument("Unknown enum value in operand: "+operand);
                    args.push_back(operand);argTypes.push_back(enumName);continue;
                }
                resolve(operand);args.push_back(expressions.at(operand));argTypes.push_back(types.at(operand));
            }
            std::string op=node.operation,resultType,expression;
            auto arity=[&](std::size_t count){if(args.size()!=count)throw std::invalid_argument("Wrong operand count for "+op);};
            const auto binary=[&](const char* symbol){
                arity(2);
                if(booleanType(argTypes[0])||booleanType(argTypes[1]))throw std::invalid_argument("Numeric operation received bool");
                resultType=argTypes[0]=="literal"?argTypes[1]:argTypes[0];
                if(resultType=="literal")resultType=returnType;
                expression="("+args[0]+" "+symbol+" "+args[1]+")";
            };
            const auto comparison=[&](const char* symbol){
                arity(2);
                if(!compatible(argTypes[0],argTypes[1]))throw std::invalid_argument("Comparison type mismatch");
                resultType="bool";
                expression="("+args[0]+" "+symbol+" "+args[1]+")";
            };
            if(op.rfind("call:",0)==0) {
                if(!sourceGenome.compositionEnabled("call"))throw std::invalid_argument("Genome rejects call composition");
                const auto targetName=op.substr(5);const auto& target=functionNamed(goal,targetName);
                if(args.size()!=target.parameters.size())throw std::invalid_argument("Goal call has wrong argument count: "+targetName);
                for(std::size_t i=0;i<args.size();++i) {
                    const bool isGeneric=!target.templateParam.empty()&&target.parameters[i].type==target.templateParam;
                    if(!isGeneric&&!compatible(target.parameters[i].type,argTypes[i]))
                        throw std::invalid_argument("Goal call type mismatch");
                }
                std::ostringstream call;call<<target.name<<'(';for(std::size_t i=0;i<args.size();++i){if(i)call<<", ";call<<args[i];}call<<')';
                expression=call.str();
                resultType=(!target.templateParam.empty()&&target.returnType==target.templateParam&&!argTypes.empty())
                    ?(argTypes[0]=="literal"?"long long":argTypes[0]):target.returnType;
                if(target.name!=ownerName)dependencies.push_back(target.name);
            } else if(op.rfind("construct:",0)==0) {
                if(!sourceGenome.compositionEnabled("construct"))throw std::invalid_argument("Genome rejects construct composition");
                const auto targetType=op.substr(10);const auto& uType=typeNamed(goal,targetType);
                if(args.size()!=uType.fields.size())throw std::invalid_argument("Construct has wrong field count for "+targetType);
                for(std::size_t i=0;i<args.size();++i)
                    if(!compatible(uType.fields[i].type,argTypes[i]))throw std::invalid_argument("Construct field type mismatch for "+targetType);
                std::ostringstream cons;cons<<uType.name<<'{';for(std::size_t i=0;i<args.size();++i){if(i)cons<<", ";cons<<args[i];}cons<<'}';
                expression=cons.str();resultType=uType.name;
                dependencies.push_back(uType.name);
            } else if(op.rfind("field:",0)==0) {
                if(!sourceGenome.compositionEnabled("field"))throw std::invalid_argument("Genome rejects field composition");
                arity(1);
                const auto fieldName=op.substr(6);
                const auto& uType=typeNamed(goal,argTypes[0]);
                const auto fIt=std::find_if(uType.fields.begin(),uType.fields.end(),[&](const auto& f){return f.name==fieldName;});
                if(fIt==uType.fields.end())throw std::invalid_argument("Unknown field "+fieldName+" on type "+uType.name);
                expression="("+args[0]+"."+fieldName+")";resultType=fIt->type;
            } else if(op.rfind("method:",0)==0) {
                if(!sourceGenome.compositionEnabled("method"))throw std::invalid_argument("Genome rejects method composition");
                if(args.empty())throw std::invalid_argument("Method call requires receiver operand");
                const auto methodName=op.substr(7);
                const auto& uType=typeNamed(goal,argTypes[0]);
                const auto mIt=std::find_if(uType.methods.begin(),uType.methods.end(),[&](const auto& m){return m.name==methodName;});
                if(mIt==uType.methods.end())throw std::invalid_argument("Unknown method "+methodName+" on type "+uType.name);
                if(args.size()-1!=mIt->parameters.size())throw std::invalid_argument("Method call has wrong argument count: "+methodName);
                for(std::size_t i=0;i<mIt->parameters.size();++i)
                    if(!compatible(mIt->parameters[i].type,argTypes[i+1]))throw std::invalid_argument("Method call argument type mismatch: "+methodName);
                std::ostringstream mcall;mcall<<"("<<args[0]<<"."<<methodName<<"(";
                for(std::size_t i=1;i<args.size();++i){if(i>1)mcall<<", ";mcall<<args[i];}
                mcall<<"))";
                expression=mcall.str();resultType=mIt->returnType;
            } else {
                if(!sourceGenome.compositionEnabled(op))throw std::invalid_argument("Genome rejects composition operator: "+op);
                if(op=="copy"){arity(1);resultType=argTypes[0]=="literal"?returnType:argTypes[0];expression=args[0];}
                else if(op=="add")binary("+");else if(op=="subtract")binary("-");else if(op=="multiply")binary("*");
                else if(op=="divide")binary("/");else if(op=="modulo")binary("%");
                else if(op=="bitwise-and")binary("&");else if(op=="bitwise-or")binary("|");else if(op=="bitwise-xor")binary("^");
                else if(op=="shift-left")binary("<<");else if(op=="shift-right")binary(">>");
                else if(op=="less")comparison("<");else if(op=="less-equal")comparison("<=");
                else if(op=="greater")comparison(">");else if(op=="greater-equal")comparison(">=");
                else if(op=="equal")comparison("==");else if(op=="not-equal")comparison("!=");
                else if(op=="logical-and"||op=="logical-or"){arity(2);if(!booleanType(argTypes[0])||!booleanType(argTypes[1]))throw std::invalid_argument("Logical operation requires bool");resultType="bool";expression="("+args[0]+(op=="logical-and"?" && ":" || ")+args[1]+")";}
                else if(op=="logical-not"){arity(1);if(!booleanType(argTypes[0]))throw std::invalid_argument("Logical not requires bool");resultType="bool";expression="(!"+args[0]+")";}
                else if(op=="negate"){arity(1);if(booleanType(argTypes[0]))throw std::invalid_argument("Negate requires numeric value");resultType=argTypes[0]=="literal"?returnType:argTypes[0];expression="(-"+args[0]+")";}
                else if(op=="abs"){arity(1);if(booleanType(argTypes[0]))throw std::invalid_argument("Abs requires numeric value");resultType=argTypes[0]=="literal"?returnType:argTypes[0];expression="(("+args[0]+" < 0) ? (-"+args[0]+") : ("+args[0]+"))";}
                else if(op=="min"){arity(2);if(!compatible(argTypes[0],argTypes[1]))throw std::invalid_argument("Min requires compatible numeric types");resultType=argTypes[0]=="literal"?argTypes[1]:argTypes[0];if(resultType=="literal")resultType=returnType;expression="(("+args[0]+" < "+args[1]+") ? ("+args[0]+") : ("+args[1]+"))";}
                else if(op=="max"){arity(2);if(!compatible(argTypes[0],argTypes[1]))throw std::invalid_argument("Max requires compatible numeric types");resultType=argTypes[0]=="literal"?argTypes[1]:argTypes[0];if(resultType=="literal")resultType=returnType;expression="(("+args[0]+" > "+args[1]+") ? ("+args[0]+") : ("+args[1]+"))";}
                else if(op=="clamp"){arity(3);if(!compatible(argTypes[0],argTypes[1])||!compatible(argTypes[0],argTypes[2]))throw std::invalid_argument("Clamp requires compatible numeric types");resultType=argTypes[0]=="literal"?returnType:argTypes[0];expression="(("+args[0]+" < "+args[1]+") ? ("+args[1]+") : (("+args[0]+" > "+args[2]+") ? ("+args[2]+") : ("+args[0]+")))";}
                else if(op=="select"){arity(3);if(!booleanType(argTypes[0])||!compatible(argTypes[1],argTypes[2]))throw std::invalid_argument("Select requires bool and compatible branches");resultType=argTypes[1]=="literal"?argTypes[2]:argTypes[1];if(resultType=="literal")resultType=returnType;expression="("+args[0]+" ? "+args[1]+" : "+args[2]+")";}
                else if(op=="switch") {
                    if(args.size()<4||(args.size()%2)!=0)
                        throw std::invalid_argument("Switch operation requires selector, at least one case/result pair, and default result");
                    resultType=argTypes.back()=="literal"?returnType:argTypes.back();
                    std::ostringstream sw;
                    sw<<"([&]() -> "<<resultType<<" { switch ("<<args[0]<<") { ";
                    for(std::size_t i=1;i+1<args.size();i+=2) {
                        if(!compatible(argTypes[0],argTypes[i]))
                            throw std::invalid_argument("Switch case label type mismatch with selector");
                        if(!compatible(resultType,argTypes[i+1]))
                            throw std::invalid_argument("Switch branch result type mismatch");
                        sw<<"case "<<args[i]<<": return "<<args[i+1]<<"; ";
                    }
                    sw<<"default: return "<<args.back()<<"; } }())";
                    expression=sw.str();
                }
                else throw std::invalid_argument("Unknown composition operator: "+op);
            }
            expressions.emplace(id,std::move(expression));types.emplace(id,std::move(resultType));active.erase(id);done.insert(id);
        };
        resolve(returnVal);
        if(!compatible(returnType,types.at(returnVal)))throw std::invalid_argument("Goal return type mismatch in "+ownerName);
        return expressions.at(returnVal);
    };

    for(const auto& type:goal.types) {
        BlueprintType bType{type.name,type.moduleName,{},{},true};
        for(const auto& f:type.fields)bType.fields.push_back({f.type,f.name});
        for(const auto& m:type.methods) {
            std::vector<std::string> deps;
            const auto expr=resolveGraph(type.name+"::"+m.name,m.returnType,m.parameters,type.fields,m.operations,m.returnValue,deps);
            BlueprintMethod bMethod{m.name,m.returnType,expr,{},true,true};
            for(const auto& p:m.parameters)bMethod.parameters.push_back({p.type,p.name});
            bType.methods.push_back(std::move(bMethod));
        }
        blueprint.types.push_back(std::move(bType));
    }

    std::ostringstream report;report<<"{\n  \"format\": \"gpi-typed-architecture-v1\",\n  \"goal_hash\": "<<goalHash;
    if(!goal.modules.empty())report<<",\n  \"modules\": "<<goal.modules.size();
    if(!goal.types.empty())report<<",\n  \"user_types\": "<<goal.types.size();
    if(!goal.enums.empty())report<<",\n  \"enums\": "<<goal.enums.size();
    if(!goal.concepts.empty())report<<",\n  \"concepts\": "<<goal.concepts.size();
    report<<",\n  \"components\": [\n";
    for(std::size_t functionIndex=0;functionIndex<goal.functions.size();++functionIndex) {
        const auto& function=goal.functions[functionIndex];
        std::vector<std::string> dependencies;
        const auto expr=resolveGraph(function.name,function.returnType,function.parameters,{},function.operations,function.returnValue,dependencies);
        BlueprintFunction generated{function.name,function.returnType,expr,{},function.moduleName,function.templateParam,function.requiresConcept,function.isConstexpr};
        for(const auto& parameter:function.parameters)generated.parameters.push_back({parameter.type,parameter.name});
        blueprint.functions.push_back(std::move(generated));
        std::sort(dependencies.begin(),dependencies.end());dependencies.erase(std::unique(dependencies.begin(),dependencies.end()),dependencies.end());
        report<<"    {\"name\": "<<json(function.name)<<", \"return_type\": "<<json(function.returnType)
              <<", \"operations\": "<<function.operations.size()<<", \"dependencies\": [";
        for(std::size_t i=0;i<dependencies.size();++i)report<<(i?", ":"")<<json(dependencies[i]);
        report<<"]}"<<(functionIndex+1==goal.functions.size()?"\n":",\n");
    }
    const auto& entry=functionNamed(goal,goal.entryFunction);blueprint.entryType=entry.returnType;
    blueprint.entryExpression=callExpression(goal,entry,goal.entryArguments);
    std::string entryExpectedCpp;
    if(!parseCaseLiteral(goal,entry.returnType,goal.entryExpected,entryExpectedCpp))
        throw std::invalid_argument("Goal entry expectation must match return type literal");
    blueprint.expectedOutput=(goal.entryExpected=="true"?"1":goal.entryExpected=="false"?"0":entryExpectedCpp)+"\n";
    blueprint.expectedError={};blueprint.expectedExitCode=0;
    std::unordered_map<std::string,std::string> expectations;
    expectations.emplace(blueprint.entryExpression,goal.entryExpected);
    for(const auto& item:goal.cases) {
        const auto& target=functionNamed(goal,item.function);const auto call=callExpression(goal,target,item.arguments);
        const std::string retType=(!target.templateParam.empty()&&target.returnType==target.templateParam)
            ?"long long":target.returnType;
        std::string expectedCpp;
        if(!parseCaseLiteral(goal,retType,item.expected,expectedCpp))
            throw std::invalid_argument("Goal case expectation must be a valid typed literal: "+item.expected);
        const auto [known,inserted]=expectations.emplace(call,item.expected);
        if(!inserted&&known->second!=item.expected)throw std::invalid_argument("Goal contains contradictory expectations for "+call);
        blueprint.tests.push_back({item.name,"("+call+") == ("+expectedCpp+")"});
    }
    report<<"  ],\n  \"entry\": "<<json(goal.entryFunction)<<",\n  \"independent_cases\": "<<goal.cases.size()<<"\n}\n";
    auto project=synthesizeProjectBlueprint(blueprint,std::move(sourceGenome));
    return {std::move(blueprint),std::move(project),report.str(),goalHash};
}
} // namespace genomic::gpi
