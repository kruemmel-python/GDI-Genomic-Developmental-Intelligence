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
bool compatible(const std::string& left,const std::string& right) {
    if(left=="literal"||right=="literal")return !booleanType(left)&&!booleanType(right);
    return booleanType(left)==booleanType(right);
}
int parseInt(const std::string& value) {
    int result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Goal integer is invalid");
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
    for(const auto& function:goal.functions) {
        hashField(hash,function.name);hashField(hash,function.returnType);
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
std::string callExpression(const GoalFunction& function,const std::vector<std::string>& arguments) {
    if(arguments.size()!=function.parameters.size())throw std::invalid_argument("Goal call has wrong argument count: "+function.name);
    std::ostringstream out;out<<function.name<<'(';for(std::size_t i=0;i<arguments.size();++i) {
        if(!integerLiteral(arguments[i])&&!boolLiteral(arguments[i]))throw std::invalid_argument("Goal case arguments must be typed literals");
        if(booleanType(function.parameters[i].type)!=boolLiteral(arguments[i]))throw std::invalid_argument("Goal case literal type mismatch");
        if(i)out<<", ";
        out<<literalSource(arguments[i]);
    }return out.str()+')';
}
void validateShape(const TypedArchitectureGoal& goal) {
    if(!identifier(goal.projectName)||!qualifiedIdentifier(goal.namespaceName)||goal.functions.empty()||
       goal.entryFunction.empty()||goal.cases.empty())throw std::invalid_argument("Goal lacks required architecture elements");
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
}

TypedArchitectureGoal loadTypedArchitectureGoal(const std::filesystem::path& goalFile) {
    if(!std::filesystem::is_regular_file(goalFile))throw std::invalid_argument("Goal file does not exist");
    std::ifstream stream(goalFile,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read typed architecture goal");
    TypedArchitectureGoal result;std::string line;std::size_t lineNumber=0;bool version=false;
    while(std::getline(stream,line)) {
        ++lineNumber;if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>65536)throw std::invalid_argument("Goal line is too long");
        if(line.empty()||line.front()=='#')continue;
        if(!version){if(line!="GPI_TYPED_ARCHITECTURE_GOAL_V1")throw std::invalid_argument("Unsupported architecture goal version");version=true;continue;}
        const auto item=split(line);const auto malformed=[&]{throw std::invalid_argument("Malformed goal line "+std::to_string(lineNumber));};
        if(item[0]=="project") {if(item.size()!=2)malformed();result.projectName=item[1];}
        else if(item[0]=="namespace") {if(item.size()!=2)malformed();result.namespaceName=item[1];}
        else if(item[0]=="function") {if(item.size()!=3)malformed();result.functions.push_back({item[1],item[2],{},{},{}});}
        else if(item[0]=="parameter") {
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
    if(!stream.eof())throw std::runtime_error("Typed architecture goal read failure");
    if(!version)throw std::invalid_argument("Typed architecture goal is empty");
    validateShape(result);return result;
}

ArchitectedProject architectTypedGoal(const TypedArchitectureGoal& goal,Cpp20SourceGenome sourceGenome) {
    validateShape(goal);
    if(!sourceGenome.structureEnabled("typed-functions")||!sourceGenome.structureEnabled("header-source-executable-test")||
       !sourceGenome.structureEnabled("independent-behavior-tests"))
        throw std::invalid_argument("Immutable genome does not authorize typed project architecture");
    ProjectBlueprint blueprint;blueprint.projectName=goal.projectName;blueprint.namespaceName=goal.namespaceName;
    blueprint.timeout=goal.timeout;blueprint.requireSandbox=goal.requireSandbox;
    const auto goalHash=semanticHash(goal);
    std::ostringstream report;report<<"{\n  \"format\": \"gpi-typed-architecture-v1\",\n  \"goal_hash\": "<<goalHash
        <<",\n  \"components\": [\n";
    for(std::size_t functionIndex=0;functionIndex<goal.functions.size();++functionIndex) {
        const auto& function=goal.functions[functionIndex];
        std::unordered_map<std::string,std::string> types,expressions;std::unordered_map<std::string,const GoalOperation*> operations;
        for(const auto& parameter:function.parameters){types.emplace(parameter.name,parameter.type);expressions.emplace(parameter.name,parameter.name);}
        for(const auto& operation:function.operations)operations.emplace(operation.id,&operation);
        std::unordered_set<std::string> active,done;std::vector<std::string> dependencies;
        std::function<void(const std::string&)> resolve=[&](const std::string& id) {
            if(done.contains(id)||types.contains(id))return;
            const auto found=operations.find(id);if(found==operations.end())throw std::invalid_argument("Goal references unknown value: "+id);
            if(!active.insert(id).second)throw std::invalid_argument("Goal operation graph contains a cycle");
            const auto& node=*found->second;std::vector<std::string> args,argTypes;
            for(const auto& operand:node.operands) {
                if(integerLiteral(operand)){args.push_back(literalSource(operand));argTypes.push_back("literal");continue;}
                if(boolLiteral(operand)){args.push_back(operand);argTypes.push_back("bool");continue;}
                resolve(operand);args.push_back(expressions.at(operand));argTypes.push_back(types.at(operand));
            }
            std::string op=node.operation,resultType,expression;auto arity=[&](std::size_t count){if(args.size()!=count)throw std::invalid_argument("Wrong operand count for "+op);};
            const auto binary=[&](const char* symbol){arity(2);if(booleanType(argTypes[0])||booleanType(argTypes[1]))throw std::invalid_argument("Numeric operation received bool");resultType=argTypes[0]=="literal"?argTypes[1]:argTypes[0];if(resultType=="literal")resultType=function.returnType;expression="("+args[0]+" "+symbol+" "+args[1]+")";};
            const auto comparison=[&](const char* symbol){arity(2);if(!compatible(argTypes[0],argTypes[1]))throw std::invalid_argument("Comparison type mismatch");resultType="bool";expression="("+args[0]+" "+symbol+" "+args[1]+")";};
            if(op.rfind("call:",0)==0) {
                if(!sourceGenome.compositionEnabled("call"))throw std::invalid_argument("Genome rejects call composition");
                const auto targetName=op.substr(5);const auto& target=functionNamed(goal,targetName);
                if(args.size()!=target.parameters.size())throw std::invalid_argument("Goal call has wrong argument count: "+targetName);
                for(std::size_t i=0;i<args.size();++i)if(!compatible(target.parameters[i].type,argTypes[i]))throw std::invalid_argument("Goal call type mismatch");
                std::ostringstream call;call<<target.name<<'(';for(std::size_t i=0;i<args.size();++i){if(i)call<<", ";call<<args[i];}call<<')';
                expression=call.str();resultType=target.returnType;if(target.name!=function.name)dependencies.push_back(target.name);
            } else {
                if(!sourceGenome.compositionEnabled(op))throw std::invalid_argument("Genome rejects composition operator: "+op);
                if(op=="copy"){arity(1);resultType=argTypes[0]=="literal"?function.returnType:argTypes[0];expression=args[0];}
                else if(op=="add")binary("+");else if(op=="subtract")binary("-");else if(op=="multiply")binary("*");
                else if(op=="divide")binary("/");else if(op=="modulo")binary("%");
                else if(op=="less")comparison("<");else if(op=="less-equal")comparison("<=");
                else if(op=="greater")comparison(">");else if(op=="greater-equal")comparison(">=");
                else if(op=="equal")comparison("==");else if(op=="not-equal")comparison("!=");
                else if(op=="logical-and"||op=="logical-or"){arity(2);if(!booleanType(argTypes[0])||!booleanType(argTypes[1]))throw std::invalid_argument("Logical operation requires bool");resultType="bool";expression="("+args[0]+(op=="logical-and"?" && ":" || ")+args[1]+")";}
                else if(op=="logical-not"){arity(1);if(!booleanType(argTypes[0]))throw std::invalid_argument("Logical not requires bool");resultType="bool";expression="(!"+args[0]+")";}
                else if(op=="negate"){arity(1);if(booleanType(argTypes[0]))throw std::invalid_argument("Negate requires numeric value");resultType=argTypes[0]=="literal"?function.returnType:argTypes[0];expression="(-"+args[0]+")";}
                else if(op=="select"){arity(3);if(!booleanType(argTypes[0])||!compatible(argTypes[1],argTypes[2]))throw std::invalid_argument("Select requires bool and compatible branches");resultType=argTypes[1]=="literal"?argTypes[2]:argTypes[1];if(resultType=="literal")resultType=function.returnType;expression="("+args[0]+" ? "+args[1]+" : "+args[2]+")";}
                else throw std::invalid_argument("Unknown composition operator: "+op);
            }
            expressions.emplace(id,std::move(expression));types.emplace(id,std::move(resultType));active.erase(id);done.insert(id);
        };
        resolve(function.returnValue);if(!compatible(function.returnType,types.at(function.returnValue)))throw std::invalid_argument("Goal return type mismatch");
        BlueprintFunction generated{function.name,function.returnType,expressions.at(function.returnValue),{}};
        for(const auto& parameter:function.parameters)generated.parameters.push_back({parameter.type,parameter.name});
        blueprint.functions.push_back(std::move(generated));
        std::sort(dependencies.begin(),dependencies.end());dependencies.erase(std::unique(dependencies.begin(),dependencies.end()),dependencies.end());
        report<<"    {\"name\": "<<json(function.name)<<", \"return_type\": "<<json(function.returnType)
              <<", \"operations\": "<<function.operations.size()<<", \"dependencies\": [";
        for(std::size_t i=0;i<dependencies.size();++i)report<<(i?", ":"")<<json(dependencies[i]);
        report<<"]}"<<(functionIndex+1==goal.functions.size()?"\n":",\n");
    }
    const auto& entry=functionNamed(goal,goal.entryFunction);blueprint.entryType=entry.returnType;
    blueprint.entryExpression=callExpression(entry,goal.entryArguments);
    if(!integerLiteral(goal.entryExpected)&&!boolLiteral(goal.entryExpected))throw std::invalid_argument("Goal entry expectation must be a typed literal");
    if(booleanType(entry.returnType)!=boolLiteral(goal.entryExpected))throw std::invalid_argument("Goal entry expectation type mismatch");
    blueprint.expectedOutput=(goal.entryExpected=="true"?"1":goal.entryExpected=="false"?"0":literalSource(goal.entryExpected))+"\n";
    blueprint.expectedError={};blueprint.expectedExitCode=0;
    std::unordered_map<std::string,std::string> expectations;
    expectations.emplace(blueprint.entryExpression,goal.entryExpected);
    for(const auto& item:goal.cases) {
        const auto& target=functionNamed(goal,item.function);const auto call=callExpression(target,item.arguments);
        if(!integerLiteral(item.expected)&&!boolLiteral(item.expected))throw std::invalid_argument("Goal case expectation must be a typed literal");
        if(booleanType(target.returnType)!=boolLiteral(item.expected))throw std::invalid_argument("Goal case expectation type mismatch");
        const auto [known,inserted]=expectations.emplace(call,item.expected);
        if(!inserted&&known->second!=item.expected)throw std::invalid_argument("Goal contains contradictory expectations for "+call);
        blueprint.tests.push_back({item.name,"("+call+") == ("+literalSource(item.expected)+")"});
    }
    report<<"  ],\n  \"entry\": "<<json(goal.entryFunction)<<",\n  \"independent_cases\": "<<goal.cases.size()<<"\n}\n";
    auto project=synthesizeProjectBlueprint(blueprint,std::move(sourceGenome));
    return {std::move(blueprint),std::move(project),report.str(),goalHash};
}
} // namespace genomic::gpi
