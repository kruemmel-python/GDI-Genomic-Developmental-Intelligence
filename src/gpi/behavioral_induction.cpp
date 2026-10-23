#include "gpi/behavioral_induction.hpp"

#include "cpp20/cpp20_lexicon.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace genomic::gpi {
namespace {
constexpr std::int64_t kBound=1000000000LL;
constexpr std::size_t kMaxNodes=2400;
constexpr std::size_t kMaxNumeric=1500;
constexpr std::size_t kMaxBoolean=450;

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start=0;
    while(true) {
        const auto end=line.find('\t',start);
        parts.push_back(line.substr(start,end==std::string::npos?end:end-start));
        if(end==std::string::npos)break;
        start=end+1;
    }
    return parts;
}
bool identifier(const std::string& value) {
    if(value.empty()||Cpp20Lexicon::isKeyword(value)||
       !(std::isalpha(static_cast<unsigned char>(value[0]))||value[0]=='_'))return false;
    return std::all_of(value.begin()+1,value.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}
bool safeType(const std::string& type) {
    return !type.empty()&&type.find_first_of(";{}#()=[]\r\n\t")==std::string::npos;
}
bool tryNumber(const std::string& value,std::int64_t& result) {
    const char* begin=value.data();
    if(!value.empty()&&value.front()=='#')++begin;
    const auto [end,ec]=std::from_chars(begin,value.data()+value.size(),result);
    return ec==std::errc{}&&end==value.data()+value.size()&&result<=kBound&&result>= -kBound;
}
std::int64_t number(const std::string& value) {
    std::int64_t result{};
    if(!tryNumber(value,result))
        throw std::invalid_argument("Behavioral value is invalid or exceeds the bounded domain: "+value);
    return result;
}
std::string json(const std::string& value) {
    std::string result="\"";
    for(const char c:value) {
        if(c=='"'||c=='\\')result+='\\';
        if(c=='\n')result+="\\n";
        else result+=c;
    }
    return result+'"';
}

// Value during multi-component & user-type induction:
// Either scalar (int64_t, or bool 0/1) or record (vector<int64_t> of fields).
struct EvalVal {
    std::string type; // "long long", "bool", or user-defined struct/enum name
    std::int64_t scalar{0};
    std::vector<std::int64_t> fields;

    bool operator==(const EvalVal& other) const {
        return type==other.type&&scalar==other.scalar&&fields==other.fields;
    }
    bool operator!=(const EvalVal& other) const { return !(*this==other); }
    bool operator<(const EvalVal& other) const {
        if(type!=other.type)return type<other.type;
        if(scalar!=other.scalar)return scalar<other.scalar;
        return fields<other.fields;
    }
};

struct Node {
    std::string op;
    std::string name;
    std::string valueType{"long long"};
    std::vector<std::size_t> operands;
    std::vector<EvalVal> outputs;
    std::size_t depth{};
};

std::string signature(const std::string& type,const std::vector<EvalVal>& values) {
    std::string key=type;
    for(const auto& val:values) {
        key+="|";
        if(val.fields.empty())key+=std::to_string(val.scalar);
        else {
            key+="{";
            for(std::size_t i=0;i<val.fields.size();++i) {
                if(i)key+=",";
                key+=std::to_string(val.fields[i]);
            }
            key+="}";
        }
    }
    return key;
}

std::optional<std::int64_t> evaluateScalar(const std::string& op,std::int64_t a,std::int64_t b=0,std::int64_t c=0) {
    std::int64_t result=0;
    if(op=="add")result=a+b;
    else if(op=="subtract")result=a-b;
    else if(op=="multiply")result=a*b;
    else if(op=="less")return a<b?1:0;
    else if(op=="greater")return a>b?1:0;
    else if(op=="less-equal")return a<=b?1:0;
    else if(op=="greater-equal")return a>=b?1:0;
    else if(op=="equal")return a==b?1:0;
    else if(op=="not-equal")return a!=b?1:0;
    else if(op=="negate")result=-a;
    else if(op=="abs")result=a<0?-a:a;
    else if(op=="min")result=a<b?a:b;
    else if(op=="max")result=a>b?a:b;
    else if(op=="clamp")result=a<b?b:(a>c?c:a);
    else if(op=="select")result=a!=0?b:c;
    else return std::nullopt;
    if(result>kBound||result< -kBound)return std::nullopt;
    return result;
}

// Parse a behavioral value token into an EvalVal and canonical architecture goal literal string
EvalVal parseEvalVal(const BehavioralGoal& goal,const std::string& expectedType,const std::string& raw,std::string& canonicalOut) {
    if(expectedType=="bool") {
        if(raw=="true"||raw=="1") { canonicalOut="true"; return {"bool",1,{}}; }
        if(raw=="false"||raw=="0") { canonicalOut="false"; return {"bool",0,{}}; }
        throw std::invalid_argument("Invalid boolean behavioral value: "+raw);
    }
    // Check enum type
    for(const auto& en:goal.enums) {
        if(en.name==expectedType) {
            std::string vName=raw;
            if(raw.rfind(en.name+"::",0)==0)vName=raw.substr(en.name.size()+2);
            for(const auto& v:en.values) {
                if(v.name==vName) {
                    canonicalOut=en.name+"::"+v.name;
                    return {en.name,v.value,{}};
                }
            }
            throw std::invalid_argument("Unknown enum value "+raw+" for enum "+en.name);
        }
    }
    // Check user-defined struct type
    for(const auto& uType:goal.types) {
        if(uType.name==expectedType) {
            std::string body=raw;
            if(body.rfind(uType.name,0)==0)body=body.substr(uType.name.size());
            if(body.size()<2||body.front()!='{'||body.back()!='}')
                throw std::invalid_argument("Struct behavioral value must have form Type{a,b}: "+raw);
            const auto inner=body.substr(1,body.size()-2);
            std::vector<std::string> parts;
            std::size_t start=0;
            while(true) {
                const auto comma=inner.find(',',start);
                parts.push_back(inner.substr(start,comma==std::string::npos?comma:comma-start));
                if(comma==std::string::npos)break;
                start=comma+1;
            }
            if(parts.size()!=uType.fields.size())
                throw std::invalid_argument("Struct behavioral value field count mismatch for "+uType.name);
            EvalVal val;val.type=uType.name;
            std::ostringstream can;can<<uType.name<<"{";
            for(std::size_t i=0;i<parts.size();++i) {
                const auto num=number(parts[i]);
                val.fields.push_back(num);
                if(i)can<<",";
                can<<"#"<<num;
            }
            can<<"}";
            canonicalOut=can.str();
            return val;
        }
    }
    // Scalar integer
    const auto num=number(raw);
    canonicalOut="#"+std::to_string(num);
    return {"long long",num,{}};
}

// Synchronize V1 fields and V2 functions list so callers inspecting either see consistent data
void normalizeBehavioralGoal(BehavioralGoal& goal) {
    if(goal.functions.empty()&&!goal.functionName.empty()) {
        BehavioralFunctionGoal fn;
        fn.name=goal.functionName;
        fn.returnType="long long";
        fn.maxDepth=goal.maxDepth;
        for(const auto& p:goal.parameters)fn.parameters.push_back({"long long",p});
        fn.samples=goal.samples;
        fn.checks=goal.checks;
        goal.functions.push_back(std::move(fn));
    } else if(!goal.functions.empty()&&goal.functionName.empty()) {
        const auto& last=goal.functions.back();
        goal.functionName=last.name;
        goal.maxDepth=last.maxDepth;
        goal.parameters.clear();
        for(const auto& p:last.parameters)goal.parameters.push_back(p.name);
        goal.samples=last.samples;
        goal.checks=last.checks;
    }
    if(goal.entryFunction.empty()&&!goal.functions.empty())
        goal.entryFunction=goal.functions.back().name;
}

BehavioralFunctionGoal* findFunction(BehavioralGoal& goal,const std::string& name) {
    for(auto& fn:goal.functions)if(fn.name==name)return &fn;
    return nullptr;
}
BehavioralTypeGoal* findType(BehavioralGoal& goal,const std::string& name) {
    for(auto& t:goal.types)if(t.name==name)return &t;
    return nullptr;
}
BehavioralMethodGoal* findMethod(BehavioralTypeGoal& type,const std::string& name) {
    for(auto& m:type.methods)if(m.name==name)return &m;
    return nullptr;
}

BehavioralExample parseExampleRow(const std::vector<std::string>& part,std::size_t firstArgIdx) {
    BehavioralExample example;
    for(std::size_t i=firstArgIdx;i+1<part.size();++i) {
        example.rawArguments.push_back(part[i]);
        std::int64_t val=0;
        if(tryNumber(part[i],val))example.arguments.push_back(val);
        else example.arguments.push_back(0);
    }
    example.rawExpected=part.back();
    std::int64_t expVal=0;
    if(tryNumber(part.back(),expVal))example.expected=expVal;
    return example;
}

} // namespace

BehavioralGoal parseBehavioralGoalStream(std::istream& in) {
    BehavioralGoal goal;
    std::string line;
    bool version=false;
    bool v2=false;
    std::size_t lineNumber=0;
    while(std::getline(in,line)) {
        ++lineNumber;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>4096)throw std::invalid_argument("Behavioral goal line too long");
        if(line.empty()||line[0]=='#')continue;
        if(!version) {
            if(line=="GDI_BEHAVIORAL_GOAL_V1") { version=true; v2=false; continue; }
            if(line=="GDI_BEHAVIORAL_GOAL_V2") { version=true; v2=true; continue; }
            throw std::invalid_argument("Unsupported behavioral goal version");
        }
        const auto part=fields(line);
        const auto malformed=[&]{throw std::invalid_argument("Malformed behavioral goal at line "+std::to_string(lineNumber));};
        if(part[0]=="project"&&part.size()==2)goal.projectName=part[1];
        else if(part[0]=="namespace"&&part.size()==2)goal.namespaceName=part[1];
        else if(part[0]=="module"&&part.size()>=2) {
            goal.modules.push_back({part[1],{part.begin()+2,part.end()}});
        } else if(part[0]=="enum"&&part.size()>=2&&part.size()<=4) {
            GoalEnum en;en.name=part[1];
            if(part.size()>=3&&!part[2].empty())en.underlyingType=part[2];
            if(part.size()==4)en.moduleName=part[3];
            goal.enums.push_back(std::move(en));
        } else if(part[0]=="enum-value"&&part.size()==4) {
            const auto found=std::find_if(goal.enums.begin(),goal.enums.end(),[&](const auto& e){return e.name==part[1];});
            if(found==goal.enums.end())malformed();
            found->values.push_back({part[2],number(part[3])});
        } else if(part[0]=="concept"&&(part.size()==4||part.size()==5)) {
            goal.concepts.push_back({part[1],part[2],part[3],part.size()==5?part[4]:""});
        } else if(part[0]=="type"&&(part.size()==2||part.size()==3)) {
            goal.types.push_back({part[1],part.size()==3?part[2]:"",{},{}});
        } else if(part[0]=="field"&&part.size()==4) {
            auto* t=findType(goal,part[1]);if(!t)malformed();
            t->fields.push_back({part[2],part[3]});
        } else if(part[0]=="method"&&(part.size()==3||part.size()==4)) {
            auto* t=findType(goal,part[1]);if(!t)malformed();
            BehavioralMethodGoal m;m.name=part[2];
            if(part.size()==4)m.returnType=part[3];
            m.maxDepth=goal.maxDepth;
            t->methods.push_back(std::move(m));
        } else if(part[0]=="method-parameter"&&(part.size()==4||part.size()==5)) {
            auto* t=findType(goal,part[1]);if(!t)malformed();
            auto* m=findMethod(*t,part[2]);if(!m)malformed();
            if(part.size()==4)m->parameters.push_back({"long long",part[3]});
            else m->parameters.push_back({part[3],part[4]});
        } else if((part[0]=="method-sample"||part[0]=="method-check")&&part.size()>=4) {
            auto* t=findType(goal,part[1]);if(!t)malformed();
            auto* m=findMethod(*t,part[2]);if(!m)malformed();
            auto ex=parseExampleRow(part,3);
            if(part[0]=="method-sample")m->samples.push_back(std::move(ex));
            else m->checks.push_back(std::move(ex));
        } else if(part[0]=="function"&&part.size()>=2&&part.size()<=6) {
            if(!v2&&part.size()==2&&goal.functions.empty()&&goal.types.empty()) {
                goal.functionName=part[1];
            } else {
                if(!goal.functionName.empty()&&goal.functions.empty()) {
                    // Migrate first function into goal.functions
                    BehavioralFunctionGoal first;first.name=goal.functionName;first.maxDepth=goal.maxDepth;
                    for(const auto& p:goal.parameters)first.parameters.push_back({"long long",p});
                    first.samples=std::move(goal.samples);first.checks=std::move(goal.checks);
                    goal.functions.push_back(std::move(first));
                }
                BehavioralFunctionGoal fn;fn.name=part[1];
                if(part.size()>=3&&!part[2].empty())fn.returnType=part[2];
                if(part.size()>=4)fn.moduleName=part[3];
                if(part.size()>=5)fn.templateParam=part[4];
                if(part.size()==6)fn.requiresConcept=part[5];
                fn.maxDepth=goal.maxDepth;
                goal.functions.push_back(std::move(fn));
                goal.functionName=part[1];
            }
        } else if(part[0]=="parameter"&&part.size()>=2&&part.size()<=4) {
            if(part.size()==2) {
                if(goal.functions.empty())goal.parameters.push_back(part[1]);
                else goal.functions.back().parameters.push_back({"long long",part[1]});
            } else if(part.size()==3) {
                auto* fn=findFunction(goal,part[1]);
                if(fn)fn->parameters.push_back({"long long",part[2]});
                else if(goal.functions.empty()&&goal.functionName==part[1])goal.parameters.push_back(part[2]);
                else malformed();
            } else {
                auto* fn=findFunction(goal,part[1]);
                if(!fn&&goal.functions.empty()&&goal.functionName==part[1]) {
                    BehavioralFunctionGoal first;first.name=goal.functionName;first.maxDepth=goal.maxDepth;
                    goal.functions.push_back(std::move(first));
                    fn=&goal.functions.back();
                }
                if(!fn)malformed();
                fn->parameters.push_back({part[2],part[3]});
            }
        } else if(part[0]=="max-depth"&&(part.size()==2||part.size()==3)) {
            if(part.size()==2) {
                const auto depth=number(part[1]);
                if(depth<1||depth>4)malformed();
                goal.maxDepth=static_cast<std::size_t>(depth);
                for(auto& fn:goal.functions)fn.maxDepth=goal.maxDepth;
            } else {
                auto* fn=findFunction(goal,part[1]);if(!fn)malformed();
                const auto depth=number(part[2]);
                if(depth<1||depth>4)malformed();
                fn->maxDepth=static_cast<std::size_t>(depth);
            }
        } else if(part[0]=="entry-function"&&part.size()==2) {
            goal.entryFunction=part[1];
        } else if((part[0]=="sample"||part[0]=="check")&&part.size()>=3) {
            // Check if part[1] is a function name
            BehavioralFunctionGoal* targetFn=findFunction(goal,part[1]);
            if(targetFn&&part.size()>=4) {
                auto example=parseExampleRow(part,2);
                if(part[0]=="sample")targetFn->samples.push_back(std::move(example));
                else targetFn->checks.push_back(std::move(example));
            } else if(!goal.functions.empty()) {
                auto& fn=goal.functions.back();
                auto example=parseExampleRow(part,1);
                if(part[0]=="sample")fn.samples.push_back(std::move(example));
                else fn.checks.push_back(std::move(example));
            } else {
                BehavioralExample example;
                for(std::size_t i=1;i+1<part.size();++i) {
                    example.rawArguments.push_back(part[i]);
                    example.arguments.push_back(number(part[i]));
                }
                example.rawExpected=part.back();
                example.expected=number(part.back());
                if(part[0]=="sample")goal.samples.push_back(std::move(example));
                else goal.checks.push_back(std::move(example));
            }
        } else malformed();
    }
    if(!version||in.bad())throw std::invalid_argument("Behavioral goal is incomplete");
    normalizeBehavioralGoal(goal);
    return goal;
}

BehavioralGoal loadBehavioralGoal(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary);
    if(!in)throw std::invalid_argument("Cannot read behavioral goal");
    return parseBehavioralGoalStream(in);
}

BehavioralGoal parseBehavioralGoalText(const std::string& text) {
    std::istringstream in(text);
    return parseBehavioralGoalStream(in);
}

namespace {
// Compiled helper representation for already-induced functions or methods so subsequent functions can compose them
struct SynthesizedCallable {
    std::string name;
    std::string returnType;
    std::vector<BehavioralParameterSpec> parameters;
    bool isMethod{false};
    std::string receiverType;
    std::function<std::optional<EvalVal>(const std::vector<EvalVal>&)> evaluate;
};

struct SingleInductionResult {
    std::vector<GoalOperation> operations;
    std::string returnValue;
    std::size_t searchNodes{0};
    std::size_t selectedDepth{0};
    std::function<std::optional<EvalVal>(const std::vector<EvalVal>&)> evaluator;
};

// Build an evaluator for an already-verified GoalFunction or GoalMethod so imported components can be composed
std::function<std::optional<EvalVal>(const std::vector<EvalVal>&)> buildGoalOperationsEvaluator(
    const BehavioralGoal& rootGoal,
    const std::string& returnType,
    const std::vector<GoalParameter>& parameters,
    const std::vector<GoalOperation>& operations,
    const std::string& returnValue,
    const std::vector<SynthesizedCallable>& priorCallables) {
    return [rootGoal,returnType,parameters,operations,returnValue,priorCallables](const std::vector<EvalVal>& callArgs)->std::optional<EvalVal> {
        if(callArgs.size()!=parameters.size())return std::nullopt;
        std::unordered_map<std::string,EvalVal> env;
        for(std::size_t i=0;i<parameters.size();++i)env.emplace(parameters[i].name,callArgs[i]);
        const auto resolveToken=[&](const std::string& tok)->std::optional<EvalVal> {
            if(const auto it=env.find(tok);it!=env.end())return it->second;
            if(tok=="true")return EvalVal{"bool",1,{}};
            if(tok=="false")return EvalVal{"bool",0,{}};
            std::int64_t num=0;
            if(!tok.empty()&&tok.front()=='#'&&tryNumber(tok,num))return EvalVal{"long long",num,{}};
            for(const auto& en:rootGoal.enums) {
                for(const auto& v:en.values)
                    if(tok==en.name+"::"+v.name)return EvalVal{en.name,v.value,{}};
            }
            return std::nullopt;
        };
        for(const auto& op:operations) {
            std::vector<EvalVal> vals;
            for(const auto& operand:op.operands) {
                auto v=resolveToken(operand);
                if(!v)return std::nullopt;
                vals.push_back(*v);
            }
            if(op.operation=="copy") {
                if(vals.size()!=1)return std::nullopt;
                env.emplace(op.id,vals[0]);
            } else if(op.operation.rfind("field:",0)==0) {
                const auto fName=op.operation.substr(6);
                bool foundField=false;
                for(const auto& uType:rootGoal.types) {
                    if(uType.name==vals[0].type) {
                        for(std::size_t f=0;f<uType.fields.size();++f)
                            if(uType.fields[f].name==fName) {
                                env.emplace(op.id,EvalVal{uType.fields[f].type,vals[0].fields[f],{}});
                                foundField=true;break;
                            }
                    }
                }
                if(!foundField)return std::nullopt;
            } else if(op.operation.rfind("construct:",0)==0) {
                const auto tName=op.operation.substr(10);
                EvalVal out{tName,0,{}};
                for(const auto& v:vals)out.fields.push_back(v.scalar);
                env.emplace(op.id,std::move(out));
            } else if(op.operation.rfind("call:",0)==0||op.operation.rfind("method:",0)==0) {
                const bool isMeth=op.operation.rfind("method:",0)==0;
                const auto targetName=op.operation.substr(isMeth?7:5);
                std::optional<EvalVal> callRes;
                for(const auto& h:priorCallables) {
                    if(h.isMethod==isMeth&&h.name==targetName){callRes=h.evaluate(vals);break;}
                }
                if(!callRes)return std::nullopt;
                env.emplace(op.id,*callRes);
            } else if(op.operation=="select") {
                if(vals.size()!=3)return std::nullopt;
                env.emplace(op.id,vals[0].scalar!=0?vals[1]:vals[2]);
            } else if(op.operation=="switch") {
                if(vals.size()<4||vals.size()%2!=0)return std::nullopt;
                EvalVal chosen=vals.back();
                for(std::size_t k=1;k+1<vals.size();k+=2) {
                    if(vals[0].scalar==vals[k].scalar){chosen=vals[k+1];break;}
                }
                env.emplace(op.id,chosen);
            } else {
                const auto res=evaluateScalar(op.operation,vals[0].scalar,vals.size()>1?vals[1].scalar:0,vals.size()>2?vals[2].scalar:0);
                if(!res)return std::nullopt;
                const bool isBool=op.operation=="less"||op.operation=="greater"||op.operation=="less-equal"||
                                  op.operation=="greater-equal"||op.operation=="equal"||op.operation=="not-equal";
                env.emplace(op.id,EvalVal{isBool?"bool":"long long",*res,{}});
            }
        }
        auto finalVal=resolveToken(returnValue);
        if(finalVal&&finalVal->type!=returnType&&returnType!="long long")finalVal->type=returnType;
        return finalVal;
    };
}

SingleInductionResult synthesizeCallableGraph(
    const BehavioralGoal& rootGoal,
    const std::string& callableName,
    const std::string& returnType,
    const std::vector<BehavioralParameterSpec>& parameters,
    const std::vector<BehavioralExample>& samples,
    const std::vector<BehavioralExample>& checks,
    std::size_t maxDepth,
    const Cpp20SourceGenome& genome,
    const std::vector<SynthesizedCallable>& priorCallables) {

    if(!identifier(callableName)||!safeType(returnType)||
       parameters.empty()||parameters.size()>4||samples.size()<4||checks.size()<2||
       samples.size()+checks.size()>64||maxDepth<1||maxDepth>4)
        throw std::invalid_argument("Behavioral target "+callableName+" requires 1..4 parameters, >=4 samples, >=2 checks and depth 1..4");

    std::set<std::string> uniqueParameters;
    for(const auto& param:parameters)
        if(!identifier(param.name)||!safeType(param.type)||!uniqueParameters.insert(param.name).second)
            throw std::invalid_argument("Invalid behavioral parameter in "+callableName);

    const auto parseExampleArgs=[&](const BehavioralExample& ex)->std::pair<std::vector<EvalVal>,EvalVal> {
        const auto arity=ex.rawArguments.empty()?ex.arguments.size():ex.rawArguments.size();
        if(arity!=parameters.size())throw std::invalid_argument("Sample/check arity mismatch in "+callableName);
        std::vector<EvalVal> argVals;
        for(std::size_t i=0;i<parameters.size();++i) {
            std::string raw=ex.rawArguments.empty()?std::to_string(ex.arguments[i]):ex.rawArguments[i];
            std::string unused;
            argVals.push_back(parseEvalVal(rootGoal,parameters[i].type,raw,unused));
        }
        std::string rawExp=ex.rawExpected.empty()?std::to_string(ex.expected):ex.rawExpected;
        std::string unused;
        EvalVal expVal=parseEvalVal(rootGoal,returnType,rawExp,unused);
        return {std::move(argVals),std::move(expVal)};
    };

    std::map<std::vector<EvalVal>,EvalVal> observations;
    std::vector<std::vector<EvalVal>> sampleArgs;
    std::vector<EvalVal> expected;
    for(const auto& example:samples) {
        auto [args,exp]=parseExampleArgs(example);
        const auto [it,inserted]=observations.emplace(args,exp);
        if(!inserted&&it->second!=exp)throw std::invalid_argument("Contradictory behavioral examples in "+callableName);
        sampleArgs.push_back(std::move(args));
        expected.push_back(std::move(exp));
    }
    std::vector<std::pair<std::vector<EvalVal>,EvalVal>> checkPairs;
    for(const auto& example:checks) {
        auto [args,exp]=parseExampleArgs(example);
        const auto found=observations.find(args);
        if(found!=observations.end()&&found->second!=exp)
            throw std::invalid_argument("Contradictory behavioral check in "+callableName);
        checkPairs.emplace_back(std::move(args),std::move(exp));
    }

    const auto n=samples.size();
    std::vector<Node> nodes;
    std::unordered_set<std::string> seen;
    std::size_t numeric=0,boolean=0;
    const auto add=[&](Node node)->bool {
        const bool isBool=node.valueType=="bool";
        if(nodes.size()>=kMaxNodes||(isBool?boolean>=kMaxBoolean:numeric>=kMaxNumeric))return false;
        if(!seen.insert(signature(node.valueType,node.outputs)).second)return false;
        if(isBool)++boolean;else ++numeric;
        nodes.push_back(std::move(node));return true;
    };

    // Depth 0: Parameters & struct field projections
    for(std::size_t p=0;p<parameters.size();++p) {
        Node node;node.op="parameter";node.name=parameters[p].name;node.valueType=parameters[p].type;node.depth=0;
        for(std::size_t s=0;s<n;++s)node.outputs.push_back(sampleArgs[s][p]);
        const std::size_t paramNodeIdx=nodes.size();
        if(add(std::move(node))) {
            // If parameter is a user-defined struct type and field access is enabled, expose its fields at depth 0/1
            if(genome.compositionEnabled("field")) {
                for(const auto& uType:rootGoal.types) {
                    if(uType.name==parameters[p].type) {
                        for(std::size_t f=0;f<uType.fields.size();++f) {
                            Node fNode;fNode.op="field:"+uType.fields[f].name;
                            fNode.operands={paramNodeIdx};
                            fNode.valueType=uType.fields[f].type;
                            fNode.depth=0;
                            for(std::size_t s=0;s<n;++s) {
                                fNode.outputs.push_back({uType.fields[f].type,sampleArgs[s][p].fields[f],{}});
                            }
                            add(std::move(fNode));
                        }
                    }
                }
            }
        }
    }

    // Depth 0: Constants
    for(const auto constant:{0LL,1LL,-1LL,2LL}) {
        Node node;node.op="constant";node.name="#"+std::to_string(constant);node.valueType="long long";
        node.outputs.assign(n,{"long long",constant,{}});
        add(std::move(node));
    }
    // Depth 0: Enum constants if returnType or any parameter is an enum
    for(const auto& en:rootGoal.enums) {
        for(const auto& val:en.values) {
            Node node;node.op="constant";node.name=en.name+"::"+val.name;node.valueType=en.name;
            node.outputs.assign(n,{en.name,val.value,{}});
            add(std::move(node));
        }
    }

    const auto findMatch=[&]()->std::optional<std::size_t> {
        for(std::size_t i=0;i<nodes.size();++i)
            if(nodes[i].valueType==returnType&&nodes[i].outputs==expected)return i;
        return std::nullopt;
    };
    std::optional<std::size_t> match=findMatch();

    const auto insertScalarOp=[&](const std::string& op,const std::vector<std::size_t>& args,std::size_t depth,bool resultBool) {
        Node node;node.op=op;node.operands=args;node.depth=depth;node.valueType=resultBool?"bool":"long long";
        for(std::size_t sample=0;sample<n;++sample) {
            const auto a=nodes[args[0]].outputs[sample].scalar;
            const auto b=args.size()>1?nodes[args[1]].outputs[sample].scalar:0;
            const auto c=args.size()>2?nodes[args[2]].outputs[sample].scalar:0;
            const auto result=evaluateScalar(op,a,b,c);
            if(!result)return;
            node.outputs.push_back({node.valueType,*result,{}});
        }
        add(std::move(node));
    };

    for(std::size_t depth=1;depth<=maxDepth&&!match;++depth) {
        const auto limit=nodes.size();
        // Unary scalar operations
        for(std::size_t a=0;a<limit;++a) {
            if(nodes[a].depth+1!=depth||nodes[a].valueType!="long long")continue;
            if(genome.compositionEnabled("negate"))insertScalarOp("negate",{a},depth,false);
            if(genome.compositionEnabled("abs"))insertScalarOp("abs",{a},depth,false);
        }

        // Prior synthesized functions and methods composition
        for(const auto& helper:priorCallables) {
            if(helper.isMethod&&!genome.compositionEnabled("method"))continue;
            if(!helper.isMethod&&!genome.compositionEnabled("call"))continue;
            if(helper.parameters.size()==1) {
                for(std::size_t a=0;a<limit;++a) {
                    if(nodes[a].depth+1!=depth||nodes[a].valueType!=helper.parameters[0].type)continue;
                    Node node;
                    node.op=helper.isMethod?("method:"+helper.name):("call:"+helper.name);
                    node.operands={a};node.depth=depth;node.valueType=helper.returnType;
                    bool ok=true;
                    for(std::size_t s=0;s<n;++s) {
                        auto res=helper.evaluate({nodes[a].outputs[s]});
                        if(!res){ok=false;break;}
                        node.outputs.push_back(*res);
                    }
                    if(ok)add(std::move(node));
                }
            } else if(helper.parameters.size()==2) {
                for(std::size_t a=0;a<limit;++a) {
                    if(nodes[a].valueType!=helper.parameters[0].type)continue;
                    for(std::size_t b=0;b<limit;++b) {
                        if(nodes[b].valueType!=helper.parameters[1].type||
                           std::max(nodes[a].depth,nodes[b].depth)+1!=depth)continue;
                        Node node;
                        node.op=helper.isMethod?("method:"+helper.name):("call:"+helper.name);
                        node.operands={a,b};node.depth=depth;node.valueType=helper.returnType;
                        bool ok=true;
                        for(std::size_t s=0;s<n;++s) {
                            auto res=helper.evaluate({nodes[a].outputs[s],nodes[b].outputs[s]});
                            if(!res){ok=false;break;}
                            node.outputs.push_back(*res);
                        }
                        if(ok)add(std::move(node));
                    }
                }
            }
        }

        // Binary scalar operations & comparisons
        for(std::size_t a=0;a<limit;++a) {
            if(nodes[a].valueType!="long long")continue;
            for(std::size_t b=0;b<limit;++b) {
                if(nodes[b].valueType!="long long"||std::max(nodes[a].depth,nodes[b].depth)+1!=depth)continue;
                for(const auto* op:{"add","subtract","multiply","min","max","less","greater","less-equal","greater-equal","equal","not-equal"}) {
                    if(!genome.compositionEnabled(op))continue;
                    if((std::string(op)=="add"||std::string(op)=="multiply"||std::string(op)=="min"||std::string(op)=="max"||
                        std::string(op)=="equal"||std::string(op)=="not-equal")&&b<a)continue;
                    const bool boolResult=std::string(op)=="less"||std::string(op)=="greater"||
                        std::string(op)=="less-equal"||std::string(op)=="greater-equal"||
                        std::string(op)=="equal"||std::string(op)=="not-equal";
                    insertScalarOp(op,{a,b},depth,boolResult);
                }
            }
        }

        // Conditional selection (supports both scalar and custom/enum return types)
        if(genome.compositionEnabled("select")) {
            const auto newLimit=nodes.size();
            for(std::size_t condition=0;condition<newLimit;++condition) {
                if(nodes[condition].valueType!="bool"||nodes[condition].depth>=depth)continue;
                for(std::size_t a=0;a<limit;++a) {
                    if(nodes[a].valueType=="bool")continue;
                    for(std::size_t b=0;b<limit;++b) {
                        if(nodes[b].valueType!=nodes[a].valueType||
                           std::max({nodes[condition].depth,nodes[a].depth,nodes[b].depth})+1!=depth)continue;
                        Node node;node.op="select";node.operands={condition,a,b};node.depth=depth;
                        node.valueType=nodes[a].valueType;
                        for(std::size_t s=0;s<n;++s) {
                            node.outputs.push_back(nodes[condition].outputs[s].scalar!=0
                                ?nodes[a].outputs[s]:nodes[b].outputs[s]);
                        }
                        add(std::move(node));
                    }
                }
            }
        }

        // Multi-way switch dispatch synthesis (when switch-dispatch and switch composition are enabled)
        if(genome.compositionEnabled("switch")&&genome.structureEnabled("switch-dispatch")&&depth==1&&!match) {
            const auto curLimit=nodes.size();
            for(std::size_t sel=0;sel<curLimit;++sel) {
                if(nodes[sel].depth!=0||nodes[sel].op!="parameter")continue;
                // Group sample indices by selector scalar value
                std::map<std::int64_t,std::vector<std::size_t>> groups;
                for(std::size_t s=0;s<n;++s)groups[nodes[sel].outputs[s].scalar].push_back(s);
                if(groups.size()<3)continue;
                // Identify constant node for each selector value
                auto findConstNode=[&](const std::string& vType,std::int64_t val)->std::optional<std::size_t> {
                    for(std::size_t c=0;c<curLimit;++c) {
                        if(nodes[c].op=="constant"&&nodes[c].valueType==vType&&
                           !nodes[c].outputs.empty()&&nodes[c].outputs[0].scalar==val)
                            return c;
                    }
                    return std::nullopt;
                };
                // Pick the last selector group as the default branch, and check if every group is matched by a candidate node
                std::vector<std::pair<std::int64_t,std::size_t>> caseBranches;
                std::optional<std::size_t> defaultBranch;
                bool allGroupsMatched=true;
                std::size_t gIdx=0;
                for(const auto& [selVal,sampleIndices]:groups) {
                    std::optional<std::size_t> branchNode;
                    for(std::size_t cand=0;cand<curLimit;++cand) {
                        if(nodes[cand].valueType!=returnType)continue;
                        bool fitsGroup=true;
                        for(std::size_t sIdx:sampleIndices) {
                            if(nodes[cand].outputs[sIdx]!=expected[sIdx]){fitsGroup=false;break;}
                        }
                        if(fitsGroup){branchNode=cand;break;}
                    }
                    if(!branchNode){allGroupsMatched=false;break;}
                    if(gIdx+1==groups.size()) {
                        defaultBranch=branchNode;
                    } else {
                        auto cNode=findConstNode(nodes[sel].valueType,selVal);
                        if(!cNode){allGroupsMatched=false;break;}
                        caseBranches.emplace_back(*cNode,*branchNode);
                    }
                    ++gIdx;
                }
                if(allGroupsMatched&&defaultBranch&&!caseBranches.empty()) {
                    Node swNode;swNode.op="switch";swNode.depth=depth;swNode.valueType=returnType;
                    swNode.operands.push_back(sel);
                    for(const auto& [cIdx,bIdx]:caseBranches) {
                        swNode.operands.push_back(cIdx);
                        swNode.operands.push_back(bIdx);
                    }
                    swNode.operands.push_back(*defaultBranch);
                    for(std::size_t s=0;s<n;++s)swNode.outputs.push_back(expected[s]);
                    add(std::move(swNode));
                    break;
                }
            }
        }

        // Struct construction if returnType is a user-defined type
        if(genome.compositionEnabled("construct")) {
            for(const auto& uType:rootGoal.types) {
                if(uType.name!=returnType)continue;
                if(uType.fields.size()==1) {
                    for(std::size_t a=0;a<nodes.size();++a) {
                        if(nodes[a].valueType!="long long")continue;
                        Node cNode;cNode.op="construct:"+uType.name;cNode.operands={a};
                        cNode.depth=depth;cNode.valueType=uType.name;
                        for(std::size_t s=0;s<n;++s)cNode.outputs.push_back({uType.name,0,{nodes[a].outputs[s].scalar}});
                        add(std::move(cNode));
                    }
                } else if(uType.fields.size()==2) {
                    const auto curSize=nodes.size();
                    for(std::size_t a=0;a<curSize;++a) {
                        if(nodes[a].valueType!="long long")continue;
                        for(std::size_t b=0;b<curSize;++b) {
                            if(nodes[b].valueType!="long long")continue;
                            Node cNode;cNode.op="construct:"+uType.name;cNode.operands={a,b};
                            cNode.depth=depth;cNode.valueType=uType.name;
                            for(std::size_t s=0;s<n;++s)
                                cNode.outputs.push_back({uType.name,0,{nodes[a].outputs[s].scalar,nodes[b].outputs[s].scalar}});
                            add(std::move(cNode));
                            if(nodes[cNode.operands[0]].outputs.size()==n&&cNode.outputs==expected)break;
                        }
                    }
                }
            }
        }

        match=findMatch();
    }
    if(!match)throw std::runtime_error("No genomically authorized expression fits the behavioral samples within the bounded search for "+callableName);

    // Build general evaluator for the matched AST node
    std::function<std::optional<EvalVal>(std::size_t,const std::vector<EvalVal>&)> evalNode=
        [&](std::size_t index,const std::vector<EvalVal>& argVals)->std::optional<EvalVal> {
            const auto& node=nodes[index];
            if(node.op=="parameter") {
                for(std::size_t i=0;i<parameters.size();++i)
                    if(parameters[i].name==node.name)return argVals[i];
                return std::nullopt;
            }
            if(node.op=="constant") {
                std::string unused;
                return parseEvalVal(rootGoal,node.valueType,node.name,unused);
            }
            std::vector<EvalVal> vals;
            for(const auto opnd:node.operands) {
                auto v=evalNode(opnd,argVals);
                if(!v)return std::nullopt;
                vals.push_back(*v);
            }
            if(node.op.rfind("field:",0)==0) {
                const auto fName=node.op.substr(6);
                for(const auto& uType:rootGoal.types) {
                    if(uType.name==vals[0].type) {
                        for(std::size_t f=0;f<uType.fields.size();++f)
                            if(uType.fields[f].name==fName)return EvalVal{uType.fields[f].type,vals[0].fields[f],{}};
                    }
                }
                return std::nullopt;
            }
            if(node.op.rfind("construct:",0)==0) {
                const auto tName=node.op.substr(10);
                EvalVal out{tName,0,{}};
                for(const auto& v:vals)out.fields.push_back(v.scalar);
                return out;
            }
            if(node.op.rfind("call:",0)==0||node.op.rfind("method:",0)==0) {
                const bool isMeth=node.op.rfind("method:",0)==0;
                const auto targetName=node.op.substr(isMeth?7:5);
                for(const auto& h:priorCallables) {
                    if(h.isMethod==isMeth&&h.name==targetName)return h.evaluate(vals);
                }
                return std::nullopt;
            }
            if(node.op=="select") {
                return vals[0].scalar!=0?vals[1]:vals[2];
            }
            if(node.op=="switch") {
                if(vals.size()<4||vals.size()%2!=0)return std::nullopt;
                for(std::size_t k=1;k+1<vals.size();k+=2) {
                    if(vals[0].scalar==vals[k].scalar)return vals[k+1];
                }
                return vals.back();
            }
            const auto res=evaluateScalar(node.op,vals[0].scalar,vals.size()>1?vals[1].scalar:0,vals.size()>2?vals[2].scalar:0);
            if(!res)return std::nullopt;
            return EvalVal{node.valueType,*res,{}};
        };

    // Verify withheld checks
    for(const auto& [chkArgs,chkExp]:checkPairs) {
        const auto observed=evalNode(*match,chkArgs);
        if(!observed||*observed!=chkExp)
            throw std::runtime_error("Synthesized candidate fails a withheld behavioral check; no project emitted");
    }

    // Emit GoalOperations
    std::vector<GoalOperation> operations;
    std::unordered_map<std::size_t,std::string> names;
    std::function<std::string(std::size_t)> emit=[&](std::size_t index)->std::string {
        const auto& node=nodes[index];
        if(node.op=="parameter"||node.op=="constant")return node.name;
        if(const auto found=names.find(index);found!=names.end())return found->second;
        std::vector<std::string> args;
        for(const auto operand:node.operands)args.push_back(emit(operand));
        const auto name="expr_"+std::to_string(operations.size()+1);
        operations.push_back({name,node.op,std::move(args)});
        names.emplace(index,name);
        return name;
    };
    std::string returnVal=emit(*match);
    if(operations.empty()) {
        operations.push_back({"expr_1","copy",{returnVal}});
        returnVal="expr_1";
    }

    const std::size_t matchedIdx=*match;
    auto boundEvaluator=[nodes,parameters,rootGoal,priorCallables,matchedIdx](const std::vector<EvalVal>& callArgs)->std::optional<EvalVal> {
        std::function<std::optional<EvalVal>(std::size_t)> ev=[&](std::size_t idx)->std::optional<EvalVal> {
            const auto& node=nodes[idx];
            if(node.op=="parameter") {
                for(std::size_t i=0;i<parameters.size();++i)
                    if(parameters[i].name==node.name)return callArgs[i];
                return std::nullopt;
            }
            if(node.op=="constant") {
                std::string unused;
                return parseEvalVal(rootGoal,node.valueType,node.name,unused);
            }
            std::vector<EvalVal> vals;
            for(const auto opnd:node.operands) {
                auto v=ev(opnd);
                if(!v)return std::nullopt;
                vals.push_back(*v);
            }
            if(node.op.rfind("field:",0)==0) {
                const auto fName=node.op.substr(6);
                for(const auto& uType:rootGoal.types) {
                    if(uType.name==vals[0].type) {
                        for(std::size_t f=0;f<uType.fields.size();++f)
                            if(uType.fields[f].name==fName)return EvalVal{uType.fields[f].type,vals[0].fields[f],{}};
                    }
                }
                return std::nullopt;
            }
            if(node.op.rfind("construct:",0)==0) {
                const auto tName=node.op.substr(10);
                EvalVal out{tName,0,{}};
                for(const auto& v:vals)out.fields.push_back(v.scalar);
                return out;
            }
            if(node.op.rfind("call:",0)==0||node.op.rfind("method:",0)==0) {
                const bool isMeth=node.op.rfind("method:",0)==0;
                const auto targetName=node.op.substr(isMeth?7:5);
                for(const auto& h:priorCallables) {
                    if(h.isMethod==isMeth&&h.name==targetName)return h.evaluate(vals);
                }
                return std::nullopt;
            }
            if(node.op=="select")return vals[0].scalar!=0?vals[1]:vals[2];
            if(node.op=="switch") {
                if(vals.size()<4||vals.size()%2!=0)return std::nullopt;
                for(std::size_t k=1;k+1<vals.size();k+=2) {
                    if(vals[0].scalar==vals[k].scalar)return vals[k+1];
                }
                return vals.back();
            }
            const auto res=evaluateScalar(node.op,vals[0].scalar,vals.size()>1?vals[1].scalar:0,vals.size()>2?vals[2].scalar:0);
            if(!res)return std::nullopt;
            return EvalVal{node.valueType,*res,{}};
        };
        return ev(matchedIdx);
    };

    return {std::move(operations),std::move(returnVal),nodes.size(),nodes[*match].depth,std::move(boundEvaluator)};
}
} // namespace

InducedArchitectureGoal induceArchitectureGoal(const BehavioralGoal& rawInput,const Cpp20SourceGenome& genome) {
    BehavioralGoal input=rawInput;
    normalizeBehavioralGoal(input);
    if(!identifier(input.projectName)||!identifier(input.namespaceName)||input.functions.empty())
        throw std::invalid_argument("Behavioral goal requires valid project, namespace and at least one function");

    TypedArchitectureGoal goal;
    goal.projectName=input.projectName;
    goal.namespaceName=input.namespaceName;
    goal.modules=input.modules;
    goal.enums=input.enums;
    goal.concepts=input.concepts;

    std::vector<SynthesizedCallable> synthesizedCallables;
    std::size_t totalSearchNodes=0;
    std::size_t maxSelectedDepth=0;
    std::size_t totalOperations=0;
    std::size_t totalSamples=0;
    std::size_t totalChecks=0;
    std::vector<std::string> reusedNames;

    // 0. Register pre-verified imported types and functions from ComponentExperienceStore / earlier stages
    for(const auto& impType:input.importedTypes) {
        if(std::any_of(input.types.begin(),input.types.end(),[&](const auto& t){return t.name==impType.name;}))continue;
        BehavioralTypeGoal bImp;bImp.name=impType.name;bImp.moduleName=impType.moduleName;bImp.fields=impType.fields;
        input.types.insert(input.types.begin(),bImp);
        goal.types.push_back(impType);
        reusedNames.push_back("type:"+impType.name);
        if(!impType.moduleName.empty()&&std::none_of(goal.modules.begin(),goal.modules.end(),[&](const auto& m){return m.name==impType.moduleName;}))
            goal.modules.push_back({impType.moduleName,{}});
        for(const auto& m:impType.methods) {
            std::vector<GoalParameter> expandedParams;
            for(const auto& f:impType.fields)expandedParams.push_back({f.type,f.name});
            for(const auto& p:m.parameters)expandedParams.push_back(p);
            auto innerEval=buildGoalOperationsEvaluator(input,m.returnType,expandedParams,m.operations,m.returnValue,synthesizedCallables);
            std::vector<BehavioralParameterSpec> callSig={{impType.name,"self"}};
            for(const auto& p:m.parameters)callSig.push_back({p.type,p.name});
            const auto fieldCount=impType.fields.size();
            const auto fieldsCopy=impType.fields;
            synthesizedCallables.push_back({
                m.name,m.returnType,std::move(callSig),true,impType.name,
                [innerEval,fieldCount,fieldsCopy](const std::vector<EvalVal>& args)->std::optional<EvalVal> {
                    if(args.empty()||args[0].fields.size()!=fieldCount)return std::nullopt;
                    std::vector<EvalVal> expanded;
                    for(std::size_t i=0;i<fieldCount;++i)
                        expanded.push_back({fieldsCopy[i].type,args[0].fields[i],{}});
                    for(std::size_t i=1;i<args.size();++i)expanded.push_back(args[i]);
                    return innerEval(expanded);
                }
            });
            reusedNames.push_back("method:"+impType.name+"::"+m.name);
        }
    }
    for(const auto& impFn:input.importedFunctions) {
        if(std::any_of(input.functions.begin(),input.functions.end(),[&](const auto& f){return f.name==impFn.name;}))continue;
        goal.functions.push_back(impFn);
        if(!impFn.moduleName.empty()&&std::none_of(goal.modules.begin(),goal.modules.end(),[&](const auto& m){return m.name==impFn.moduleName;}))
            goal.modules.push_back({impFn.moduleName,{}});
        std::vector<BehavioralParameterSpec> bParams;
        for(const auto& p:impFn.parameters)bParams.push_back({p.type=="T"?"long long":p.type,p.name});
        const std::string evalRet=(impFn.returnType=="T")?"long long":impFn.returnType;
        std::vector<GoalParameter> evalParams;
        for(const auto& p:impFn.parameters)evalParams.push_back({p.type=="T"?"long long":p.type,p.name});
        auto fnEval=buildGoalOperationsEvaluator(input,evalRet,evalParams,impFn.operations,impFn.returnValue,synthesizedCallables);
        synthesizedCallables.push_back({impFn.name,evalRet,std::move(bParams),false,"",std::move(fnEval)});
        reusedNames.push_back("function:"+impFn.name);
    }

    // 1. Induce methods on user-defined types if any have behavioral examples
    for(const auto& bType:input.types) {
        if(std::any_of(goal.types.begin(),goal.types.end(),[&](const auto& gt){return gt.name==bType.name;}))continue;
        GoalType gType;
        gType.name=bType.name;
        gType.moduleName=bType.moduleName;
        gType.fields=bType.fields;
        for(const auto& bMethod:bType.methods) {
            // For method induction, implicit fields are parameters so the synthesizer can access them directly
            std::vector<BehavioralParameterSpec> methodParams;
            for(const auto& f:bType.fields)methodParams.push_back({f.type,f.name});
            for(const auto& p:bMethod.parameters)methodParams.push_back(p);

            auto res=synthesizeCallableGraph(input,bMethod.name,bMethod.returnType,methodParams,
                bMethod.samples,bMethod.checks,bMethod.maxDepth,genome,synthesizedCallables);
            totalSearchNodes+=res.searchNodes;
            maxSelectedDepth=std::max(maxSelectedDepth,res.selectedDepth);
            totalOperations+=res.operations.size();
            totalSamples+=bMethod.samples.size();
            totalChecks+=bMethod.checks.size();

            GoalMethod gMethod;
            gMethod.name=bMethod.name;
            gMethod.returnType=bMethod.returnType;
            for(const auto& p:bMethod.parameters)gMethod.parameters.push_back({p.type,p.name});
            gMethod.operations=std::move(res.operations);
            gMethod.returnValue=std::move(res.returnValue);
            gType.methods.push_back(std::move(gMethod));

            // Register method as a callable taking (StructInstance, methodParams...)
            std::vector<BehavioralParameterSpec> callSig={{bType.name,"self"}};
            for(const auto& p:bMethod.parameters)callSig.push_back(p);
            const auto fieldCount=bType.fields.size();
            const auto fieldsCopy=bType.fields;
            auto innerEval=res.evaluator;
            synthesizedCallables.push_back({
                bMethod.name,bMethod.returnType,std::move(callSig),true,bType.name,
                [innerEval,fieldCount,fieldsCopy](const std::vector<EvalVal>& args)->std::optional<EvalVal> {
                    if(args.empty()||args[0].fields.size()!=fieldCount)return std::nullopt;
                    std::vector<EvalVal> expanded;
                    for(std::size_t i=0;i<fieldCount;++i)
                        expanded.push_back({fieldsCopy[i].type,args[0].fields[i],{}});
                    for(std::size_t i=1;i<args.size();++i)expanded.push_back(args[i]);
                    return innerEval(expanded);
                }
            });
        }
        goal.types.push_back(std::move(gType));
    }

    // 2. Induce functions in topological order, allowing later functions to call earlier ones!
    for(const auto& bFn:input.functions) {
        auto res=synthesizeCallableGraph(input,bFn.name,bFn.returnType,bFn.parameters,
            bFn.samples,bFn.checks,bFn.maxDepth,genome,synthesizedCallables);
        totalSearchNodes+=res.searchNodes;
        maxSelectedDepth=std::max(maxSelectedDepth,res.selectedDepth);
        totalOperations+=res.operations.size();
        totalSamples+=bFn.samples.size();
        totalChecks+=bFn.checks.size();

        GoalFunction function;
        function.name=bFn.name;
        function.returnType=bFn.returnType;
        function.moduleName=bFn.moduleName;
        function.templateParam=bFn.templateParam;
        function.requiresConcept=bFn.requiresConcept;
        for(const auto& p:bFn.parameters) {
            const std::string pType=(!bFn.templateParam.empty()&&p.type=="long long")?bFn.templateParam:p.type;
            function.parameters.push_back({pType,p.name});
        }
        if(!bFn.templateParam.empty()&&function.returnType=="long long")
            function.returnType=bFn.templateParam;
        function.operations=std::move(res.operations);
        function.returnValue=std::move(res.returnValue);
        goal.functions.push_back(std::move(function));

        synthesizedCallables.push_back({
            bFn.name,bFn.returnType,bFn.parameters,false,"",std::move(res.evaluator)
        });

        const auto formatCase=[&](const BehavioralExample& ex,const std::string& prefix,std::size_t idx)->GoalCase {
            GoalCase test;
            test.name=(input.functions.size()==1?prefix:(bFn.name+"_"+prefix))+"_"+std::to_string(idx);
            test.function=bFn.name;
            std::string rawExp=ex.rawExpected.empty()?std::to_string(ex.expected):ex.rawExpected;
            std::string canExp;
            parseEvalVal(input,bFn.returnType,rawExp,canExp);
            test.expected=canExp;
            const auto arity=ex.rawArguments.empty()?ex.arguments.size():ex.rawArguments.size();
            for(std::size_t a=0;a<arity;++a) {
                std::string rawArg=ex.rawArguments.empty()?std::to_string(ex.arguments[a]):ex.rawArguments[a];
                std::string canArg;
                parseEvalVal(input,bFn.parameters[a].type,rawArg,canArg);
                test.arguments.push_back(canArg);
            }
            return test;
        };

        std::size_t localS=0,localC=0;
        for(const auto& sample:bFn.samples) {
            goal.cases.push_back(formatCase(sample,"sample",++localS));
        }
        for(const auto& check:bFn.checks) {
            goal.cases.push_back(formatCase(check,"check",++localC));
        }
    }

    // Set entry function (defaults to last function)
    const std::string entryName=input.entryFunction.empty()?input.functions.back().name:input.entryFunction;
    const auto bEntryIt=std::find_if(input.functions.begin(),input.functions.end(),[&](const auto& f){return f.name==entryName;});
    const auto& bEntry=(bEntryIt!=input.functions.end())?*bEntryIt:input.functions.back();
    goal.entryFunction=bEntry.name;
    const auto& entrySample=bEntry.samples.front();
    {
        std::string rawExp=entrySample.rawExpected.empty()?std::to_string(entrySample.expected):entrySample.rawExpected;
        parseEvalVal(input,bEntry.returnType,rawExp,goal.entryExpected);
        const auto arity=entrySample.rawArguments.empty()?entrySample.arguments.size():entrySample.rawArguments.size();
        for(std::size_t a=0;a<arity;++a) {
            std::string rawArg=entrySample.rawArguments.empty()?std::to_string(entrySample.arguments[a]):entrySample.rawArguments[a];
            std::string canArg;
            parseEvalVal(input,bEntry.parameters[a].type,rawArg,canArg);
            goal.entryArguments.push_back(canArg);
        }
    }

    std::ostringstream report;
    report<<"{\n  \"format\": \"gdi-behavioral-induction-v1\",\n  \"genome_hash\": "<<genome.genomeHash()
          <<",\n  \"function\": "<<json(goal.entryFunction)
          <<",\n  \"functions_induced\": "<<goal.functions.size()
          <<",\n  \"reused_components\": "<<reusedNames.size()
          <<",\n  \"user_types\": "<<goal.types.size()
          <<",\n  \"modules\": "<<goal.modules.size()
          <<",\n  \"search_nodes\": "<<totalSearchNodes
          <<",\n  \"selected_depth\": "<<maxSelectedDepth
          <<",\n  \"operations\": "<<totalOperations
          <<",\n  \"training_samples\": "<<totalSamples
          <<",\n  \"withheld_checks\": "<<totalChecks
          <<",\n  \"withheld_checks_passed\": true\n}\n";
    return {std::move(goal),report.str(),totalSearchNodes,reusedNames.size(),std::move(reusedNames)};
}

std::string serializeTypedArchitectureGoal(const TypedArchitectureGoal& goal) {
    std::ostringstream out;
    const bool v2=!goal.modules.empty()||!goal.types.empty()||!goal.enums.empty()||!goal.concepts.empty();
    out<<(v2?"GPI_TYPED_ARCHITECTURE_GOAL_V2":"GPI_TYPED_ARCHITECTURE_GOAL_V1")
       <<"\nproject\t"<<goal.projectName
       <<"\nnamespace\t"<<goal.namespaceName<<'\n';
    for(const auto& mod:goal.modules) {
        out<<"module\t"<<mod.name;
        for(const auto& dep:mod.dependencies)out<<'\t'<<dep;
        out<<'\n';
    }
    for(const auto& en:goal.enums) {
        out<<"enum\t"<<en.name<<'\t'<<en.underlyingType;
        if(!en.moduleName.empty())out<<'\t'<<en.moduleName;
        out<<'\n';
        for(const auto& val:en.values)out<<"enum-value\t"<<en.name<<'\t'<<val.name<<'\t'<<val.value<<'\n';
    }
    for(const auto& cpt:goal.concepts) {
        out<<"concept\t"<<cpt.name<<'\t'<<cpt.param<<'\t'<<cpt.expression;
        if(!cpt.moduleName.empty())out<<'\t'<<cpt.moduleName;
        out<<'\n';
    }
    for(const auto& type:goal.types) {
        out<<"type\t"<<type.name;
        if(!type.moduleName.empty())out<<'\t'<<type.moduleName;
        out<<'\n';
        for(const auto& f:type.fields)out<<"field\t"<<type.name<<'\t'<<f.type<<'\t'<<f.name<<'\n';
        for(const auto& m:type.methods) {
            out<<"method\t"<<type.name<<'\t'<<m.name<<'\t'<<m.returnType<<'\n';
            for(const auto& p:m.parameters)out<<"method-parameter\t"<<type.name<<'\t'<<m.name<<'\t'<<p.type<<'\t'<<p.name<<'\n';
            for(const auto& op:m.operations) {
                out<<"method-operation\t"<<type.name<<'\t'<<m.name<<'\t'<<op.id<<'\t'<<op.operation;
                for(const auto& arg:op.operands)out<<'\t'<<arg;
                out<<'\n';
            }
            out<<"method-return\t"<<type.name<<'\t'<<m.name<<'\t'<<m.returnValue<<'\n';
        }
    }
    for(const auto& function:goal.functions) {
        out<<"function\t"<<function.name<<'\t'<<function.returnType;
        if(!function.moduleName.empty()||!function.templateParam.empty()||!function.requiresConcept.empty()) {
            out<<'\t'<<function.moduleName;
            if(!function.templateParam.empty()||!function.requiresConcept.empty()) {
                out<<'\t'<<function.templateParam;
                if(!function.requiresConcept.empty())out<<'\t'<<function.requiresConcept;
            }
        }
        out<<'\n';
        for(const auto& param:function.parameters)out<<"parameter\t"<<function.name<<'\t'<<param.type<<'\t'<<param.name<<'\n';
        for(const auto& op:function.operations) {
            out<<"operation\t"<<function.name<<'\t'<<op.id<<'\t'<<op.operation;
            for(const auto& arg:op.operands)out<<'\t'<<arg;
            out<<'\n';
        }
        out<<"return\t"<<function.name<<'\t'<<function.returnValue<<'\n';
    }
    out<<"entry\t"<<goal.entryFunction<<'\t'<<goal.entryExpected;
    for(const auto& arg:goal.entryArguments)out<<'\t'<<arg;
    out<<'\n';
    for(const auto& item:goal.cases) {
        out<<"case\t"<<item.name<<'\t'<<item.function<<'\t'<<item.expected;
        for(const auto& arg:item.arguments)out<<'\t'<<arg;
        out<<'\n';
    }
    out<<"timeout-seconds\t"<<goal.timeout.count()<<"\nrequire-sandbox\ttrue\n";
    return out.str();
}
} // namespace genomic::gpi
