#include "genome/genomic_rule_engine.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace genomic {

static TypeKind stringToTypeKind(const std::string& s) {
    if (s == "void") return TypeKind::Void;
    if (s == "bool") return TypeKind::Bool;
    if (s == "char") return TypeKind::Char;
    if (s == "char8_t") return TypeKind::Char8;
    if (s == "char16_t") return TypeKind::Char16;
    if (s == "char32_t") return TypeKind::Char32;
    if (s == "wchar_t") return TypeKind::WChar;
    if (s == "int8_t") return TypeKind::Int8;
    if (s == "int16_t") return TypeKind::Int16;
    if (s == "int32_t" || s == "int") return TypeKind::Int32;
    if (s == "int64_t" || s == "long long") return TypeKind::Int64;
    if (s == "uint8_t") return TypeKind::UInt8;
    if (s == "uint16_t") return TypeKind::UInt16;
    if (s == "uint32_t" || s == "unsigned int") return TypeKind::UInt32;
    if (s == "uint64_t" || s == "unsigned long long") return TypeKind::UInt64;
    if (s == "float") return TypeKind::Float32;
    if (s == "double") return TypeKind::Float64;
    if (s == "pointer") return TypeKind::Pointer;
    if (s == "lvalue_ref") return TypeKind::LValueReference;
    if (s == "rvalue_ref") return TypeKind::RValueReference;
    if (s == "record") return TypeKind::Record;
    if (s == "auto") return TypeKind::Auto;
    if (s == "array") return TypeKind::Array;
    if (s == "function") return TypeKind::Function;
    throw std::invalid_argument("Unknown genomic C++ type: " + s);
}

static std::string typeKindToString(TypeKind k) {
    switch (k) {
        case TypeKind::Void: return "void";
        case TypeKind::Bool: return "bool";
        case TypeKind::Char: return "char";
        case TypeKind::Char8: return "char8_t";
        case TypeKind::Char16: return "char16_t";
        case TypeKind::Char32: return "char32_t";
        case TypeKind::WChar: return "wchar_t";
        case TypeKind::Int8: return "int8_t";
        case TypeKind::Int16: return "int16_t";
        case TypeKind::Int32: return "int32_t";
        case TypeKind::Int64: return "int64_t";
        case TypeKind::UInt8: return "uint8_t";
        case TypeKind::UInt16: return "uint16_t";
        case TypeKind::UInt32: return "uint32_t";
        case TypeKind::UInt64: return "uint64_t";
        case TypeKind::Float32: return "float";
        case TypeKind::Float64: return "double";
        case TypeKind::Pointer: return "pointer";
        case TypeKind::LValueReference: return "lvalue_ref";
        case TypeKind::RValueReference: return "rvalue_ref";
        case TypeKind::Record: return "record";
        case TypeKind::Auto: return "auto";
        case TypeKind::Array: return "array";
        case TypeKind::Function: return "function";
        case TypeKind::TemplateParam: return "template_param";
    }
    return "unknown";
}

static IrOpcode stringToIrOpcode(const std::string& str) {
    if (str == "Nop") return IrOpcode::Nop;
    if (str == "ConstInt") return IrOpcode::ConstInt;
    if (str == "ConstFloat") return IrOpcode::ConstFloat;
    if (str == "ConstBool") return IrOpcode::ConstBool;
    if (str == "ConstString") return IrOpcode::ConstString;
    if (str == "Load") return IrOpcode::Load;
    if (str == "Store") return IrOpcode::Store;
    if (str == "Add") return IrOpcode::Add;
    if (str == "Sub") return IrOpcode::Sub;
    if (str == "Mul") return IrOpcode::Mul;
    if (str == "Div") return IrOpcode::Div;
    if (str == "Mod") return IrOpcode::Mod;
    if (str == "BitAnd") return IrOpcode::BitAnd;
    if (str == "BitOr") return IrOpcode::BitOr;
    if (str == "BitXor") return IrOpcode::BitXor;
    if (str == "BitShl") return IrOpcode::BitShl;
    if (str == "BitShr") return IrOpcode::BitShr;
    if (str == "CmpEQ") return IrOpcode::CmpEQ;
    if (str == "CmpNE") return IrOpcode::CmpNE;
    if (str == "CmpLT") return IrOpcode::CmpLT;
    if (str == "CmpLE") return IrOpcode::CmpLE;
    if (str == "CmpGT") return IrOpcode::CmpGT;
    if (str == "CmpGE") return IrOpcode::CmpGE;
    if (str == "Branch") return IrOpcode::Branch;
    if (str == "BranchCond") return IrOpcode::BranchCond;
    if (str == "Call") return IrOpcode::Call;
    if (str == "Return") return IrOpcode::Return;
    if (str == "Alloca") return IrOpcode::Alloca;
    if (str == "EmitMolecule") return IrOpcode::EmitMolecule;
    if (str == "Halt") return IrOpcode::Halt;
    if (str == "Trap") return IrOpcode::Trap;
    if (str == "Suspend") return IrOpcode::Suspend;
    throw std::invalid_argument("Unknown genomic IR opcode: " + str);
}

static std::string irOpcodeToMnemonic(IrOpcode op) {
    switch (op) {
        case IrOpcode::Nop: return "Nop";
        case IrOpcode::ConstInt: return "ConstInt";
        case IrOpcode::ConstFloat: return "ConstFloat";
        case IrOpcode::ConstBool: return "ConstBool";
        case IrOpcode::ConstString: return "ConstString";
        case IrOpcode::Alloca: return "Alloca";
        case IrOpcode::Load: return "Load";
        case IrOpcode::Store: return "Store";
        case IrOpcode::Add: return "Add";
        case IrOpcode::Sub: return "Sub";
        case IrOpcode::Mul: return "Mul";
        case IrOpcode::Div: return "Div";
        case IrOpcode::Mod: return "Mod";
        case IrOpcode::BitAnd: return "BitAnd";
        case IrOpcode::BitOr: return "BitOr";
        case IrOpcode::BitXor: return "BitXor";
        case IrOpcode::BitShl: return "BitShl";
        case IrOpcode::BitShr: return "BitShr";
        case IrOpcode::CmpEQ: return "CmpEQ";
        case IrOpcode::CmpNE: return "CmpNE";
        case IrOpcode::CmpLT: return "CmpLT";
        case IrOpcode::CmpLE: return "CmpLE";
        case IrOpcode::CmpGT: return "CmpGT";
        case IrOpcode::CmpGE: return "CmpGE";
        case IrOpcode::Branch: return "Branch";
        case IrOpcode::BranchCond: return "BranchCond";
        case IrOpcode::Call: return "Call";
        case IrOpcode::Return: return "Return";
        case IrOpcode::Suspend: return "Suspend";
        case IrOpcode::Resume: return "Resume";
        case IrOpcode::EmitMolecule: return "EmitMolecule";
        case IrOpcode::ReadMolecule: return "ReadMolecule";
        case IrOpcode::CheckCapability: return "CheckCapability";
        case IrOpcode::Print: return "Print";
        case IrOpcode::Halt: return "Halt";
        case IrOpcode::Trap: return "Trap";
    }
    return "Nop";
}

void GenomicRuleEngine::loadRulesFromGenome(const GenomeStrand& strand) {
    typeDefinitions_.clear();
    binaryOpRules_.clear();
    unaryOpRules_.clear();
    conversionRules_.clear();
    arithmeticConversionRules_.clear();
    constraintRules_.clear();
    loweringRules_.clear();

    // Legacy test genomes may contain only a single semantic rule region.
    // A real CppGrammar region must be complete and is interpreted strictly.
    expressionGrammar_ = GenomicExpressionGrammar::standardCpp20();
    for (const auto& region : strand.getAllRegions()) {
        if (region.kind == RegionKind::CppGrammar || region.kind == RegionKind::CppGrammarRule) {
            expressionGrammar_ = GenomicExpressionGrammar::fromGenome(strand);
            break;
        }
    }

    for (const auto& reg : strand.getAllRegions()) {
        std::string payloadStr(reinterpret_cast<const char*>(reg.payload.data()), reg.payload.size());

        if (reg.kind == RegionKind::TypeSystem || reg.kind == RegionKind::CppTypeRule) {
            parseTypeRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppSemanticRule || reg.kind == RegionKind::SemanticRules) {
            parseSemanticRulePayload(payloadStr);
            parseUnaryRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppConversionRule) {
            parseConversionRulePayload(payloadStr);
            parseArithmeticConversionPayload(payloadStr);
        } else if (reg.kind == RegionKind::CppConstraintRule) {
            parseConstraintRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CompilerLogic || reg.kind == RegionKind::CppLoweringRule) {
            parseLoweringRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppLookupRule) {
            parseLookupRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppOverloadRule) {
            parseOverloadRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppLifetimeRule) {
            parseLifetimeRulePayload(payloadStr);
        } else if (reg.kind == RegionKind::CppMemoryModelRule) {
            parseMemoryModelPayload(payloadStr);
        }
    }
}

void GenomicRuleEngine::parseTypeRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("type:", 0) == 0) {
            // Format: type:NAME:SIZE:ALIGN:INTEGRAL:FLOAT:SIGNED:PTR:REF
            std::istringstream tStream(token.substr(5));
            std::string name, sizeStr, alignStr, intStr, fltStr, sgnStr, ptrStr, refStr;
            if (std::getline(tStream, name, ':') &&
                std::getline(tStream, sizeStr, ':') &&
                std::getline(tStream, alignStr, ':') &&
                std::getline(tStream, intStr, ':') &&
                std::getline(tStream, fltStr, ':') &&
                std::getline(tStream, sgnStr, ':')) {
                GenomicTypeInfo info;
                info.name = name;
                info.kind = stringToTypeKind(name);
                info.sizeBytes = static_cast<uint32_t>(std::stoul(sizeStr));
                info.alignment = static_cast<uint32_t>(std::stoul(alignStr));
                info.isIntegral = (intStr == "1" || intStr == "true");
                info.isFloatingPoint = (fltStr == "1" || fltStr == "true");
                info.isSigned = (sgnStr == "1" || sgnStr == "true");
                if (std::getline(tStream, ptrStr, ':')) info.isPointer = (ptrStr == "1");
                if (std::getline(tStream, refStr, ':')) info.isReference = (refStr == "1");

                typeDefinitions_[static_cast<int>(info.kind)] = info;
            }
        }
    }
}

void GenomicRuleEngine::parseSemanticRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("binop:", 0) == 0) {
            // Format: binop:OP:LHS:RHS:RES:VALCAT:LOWERING:CONSTEXPR
            std::istringstream bStream(token.substr(6));
            std::string op, lhsStr, rhsStr, resStr, valCatStr, lowStr, cexprStr;
            if (std::getline(bStream, op, ':') &&
                std::getline(bStream, lhsStr, ':') &&
                std::getline(bStream, rhsStr, ':') &&
                std::getline(bStream, resStr, ':')) {
                GenomicBinaryOpRule r;
                r.op = op;
                r.lhsKind = stringToTypeKind(lhsStr);
                r.rhsKind = stringToTypeKind(rhsStr);
                r.resultKind = stringToTypeKind(resStr);
                r.resultValueCategory = ValueCategory::PRValue;

                if (std::getline(bStream, valCatStr, ':')) {
                    if (valCatStr == "LValue") r.resultValueCategory = ValueCategory::LValue;
                    else if (valCatStr == "XValue") r.resultValueCategory = ValueCategory::XValue;
                }

                if (std::getline(bStream, lowStr, ':')) {
                    r.loweringOpcode = stringToIrOpcode(lowStr);
                } else {
                    r.loweringOpcode = IrOpcode::Add;
                }

                if (std::getline(bStream, cexprStr, ':')) {
                    r.isConstexprLegal = (cexprStr == "1" || cexprStr == "true");
                }

                binaryOpRules_.push_back(r);
            }
        }
    }
}

void GenomicRuleEngine::parseUnaryRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("unop:", 0) == 0) {
            // Format: unop:OP:OPERAND:RES:PREFIX:VALCAT:LOWERING:CONSTEXPR
            std::istringstream uStream(token.substr(5));
            std::string op, opndStr, resStr, pfxStr, valCatStr, lowStr, cexprStr;
            if (std::getline(uStream, op, ':') &&
                std::getline(uStream, opndStr, ':') &&
                std::getline(uStream, resStr, ':') &&
                std::getline(uStream, pfxStr, ':')) {
                GenomicUnaryOpRule r;
                r.op = op;
                r.operandKind = stringToTypeKind(opndStr);
                r.resultKind = stringToTypeKind(resStr);
                r.isPrefix = (pfxStr == "1" || pfxStr == "true");
                r.resultValueCategory = ValueCategory::PRValue;

                if (std::getline(uStream, valCatStr, ':')) {
                    if (valCatStr == "LValue") r.resultValueCategory = ValueCategory::LValue;
                }
                if (std::getline(uStream, lowStr, ':')) {
                    r.loweringOpcode = stringToIrOpcode(lowStr);
                }
                if (std::getline(uStream, cexprStr, ':')) {
                    r.isConstexprLegal = (cexprStr == "1" || cexprStr == "true");
                }
                unaryOpRules_.push_back(r);
            }
        }
    }
}

void GenomicRuleEngine::parseConversionRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("conv:", 0) == 0) {
            // Format: conv:FROM:TO:COST:ALLOWED:CATEGORY
            std::istringstream cStream(token.substr(5));
            std::string fromStr, toStr, costStr, allowedStr, catStr;
            if (std::getline(cStream, fromStr, ':') &&
                std::getline(cStream, toStr, ':') &&
                std::getline(cStream, costStr, ':') &&
                std::getline(cStream, allowedStr, ':')) {
                GenomicConversionRule r;
                r.fromKind = stringToTypeKind(fromStr);
                r.toKind = stringToTypeKind(toStr);
                r.conversionCost = std::stoi(costStr);
                r.isImplicitAllowed = (allowedStr == "1" || allowedStr == "true");
                if (std::getline(cStream, catStr, ':')) {
                    r.category = catStr;
                }
                conversionRules_.push_back(r);
            }
        }
    }
}

void GenomicRuleEngine::parseArithmeticConversionPayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("uac:", 0) == 0) {
            // Format: uac:LHS:RHS:COMMON
            std::istringstream aStream(token.substr(4));
            std::string lhsStr, rhsStr, comStr;
            if (std::getline(aStream, lhsStr, ':') &&
                std::getline(aStream, rhsStr, ':') &&
                std::getline(aStream, comStr, ':')) {
                GenomicArithmeticConversionRule r;
                r.lhsKind = stringToTypeKind(lhsStr);
                r.rhsKind = stringToTypeKind(rhsStr);
                r.commonKind = stringToTypeKind(comStr);
                arithmeticConversionRules_.push_back(r);
            }
        }
    }
}

void GenomicRuleEngine::parseConstraintRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("concept:", 0) == 0) {
            // Format: concept:NAME:TYPES:SUBSUMES
            std::istringstream cStream(token.substr(8));
            std::string name, typeList, subsumeList;
            if (std::getline(cStream, name, ':') && std::getline(cStream, typeList, ':')) {
                GenomicConstraintRule r;
                r.conceptName = name;
                std::istringstream tlStream(typeList);
                std::string tStr;
                while (std::getline(tlStream, tStr, ',')) {
                    if (!tStr.empty()) {
                        r.satisfiedTypes.push_back(stringToTypeKind(tStr));
                    }
                }
                if (std::getline(cStream, subsumeList, ':')) {
                    std::istringstream subStream(subsumeList);
                    std::string sStr;
                    while (std::getline(subStream, sStr, ',')) {
                        if (!sStr.empty()) {
                            r.subsumedConcepts.push_back(sStr);
                        }
                    }
                }
                constraintRules_[name] = r;
            }
        }
    }
}

void GenomicRuleEngine::parseLoweringRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("lowering:", 0) == 0) {
            // Format: lowering:CONSTRUCT:OPCODE (where construct may contain colons, e.g. binop:+)
            std::string body = token.substr(9);
            size_t lastColon = body.rfind(':');
            if (lastColon != std::string::npos) {
                std::string construct = body.substr(0, lastColon);
                std::string opStr = body.substr(lastColon + 1);
                loweringRules_[construct] = stringToIrOpcode(opStr);
            }
        }
    }
}

void GenomicRuleEngine::parseLookupRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("lookup:", 0) == 0) {
            // Format: lookup:scopes=local,block,class...:shadow=1:twophase=1
            std::istringstream lStream(token.substr(7));
            std::string part;
            while (std::getline(lStream, part, ':')) {
                if (part.rfind("scopes=", 0) == 0) {
                    std::istringstream sStream(part.substr(7));
                    std::string sc;
                    lookupRule_.scopeSearchOrder.clear();
                    while (std::getline(sStream, sc, ',')) {
                        lookupRule_.scopeSearchOrder.push_back(sc);
                    }
                } else if (part == "shadow=0") {
                    lookupRule_.allowShadowing = false;
                } else if (part == "twophase=1") {
                    lookupRule_.twoPhaseLookup = true;
                }
            }
        }
    }
}

void GenomicRuleEngine::parseOverloadRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("overload:", 0) == 0) {
            std::istringstream oStream(token.substr(9));
            std::string part;
            while (std::getline(oStream, part, ':')) {
                if (part.rfind("exact=", 0) == 0) overloadRule_.exactMatchScore = std::stoi(part.substr(6));
                else if (part.rfind("prom=", 0) == 0) overloadRule_.promotionScore = std::stoi(part.substr(5));
                else if (part.rfind("conv=", 0) == 0) overloadRule_.conversionScore = std::stoi(part.substr(5));
                else if (part.rfind("user=", 0) == 0) overloadRule_.userDefinedScore = std::stoi(part.substr(5));
                else if (part == "prefer_nontemplate=1") overloadRule_.preferNonTemplateOverSpecialization = true;
                else if (part == "prefer_constrained=1") overloadRule_.preferMoreConstrained = true;
            }
        }
    }
}

void GenomicRuleEngine::parseLifetimeRulePayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("lifetime:", 0) == 0) {
            std::istringstream lStream(token.substr(9));
            std::string part;
            while (std::getline(lStream, part, ':')) {
                if (part == "raii=1") lifetimeRule_.enforceDestructorsOnScopeExit = true;
                else if (part == "temp_lifetime=1") lifetimeRule_.trackTemporaryLifetimes = true;
                else if (part == "uaf_guard=1") lifetimeRule_.guardAgainstUseAfterFree = true;
                else if (part == "prvalue_materialize=1") lifetimeRule_.materializePRValue = true;
            }
        }
    }
}

void GenomicRuleEngine::parseMemoryModelPayload(const std::string& payload) {
    std::istringstream stream(payload);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.rfind("memmodel:", 0) == 0) {
            std::istringstream mStream(token.substr(9));
            std::string part;
            while (std::getline(mStream, part, ':')) {
                if (part == "race_detector=1") memoryModelRule_.detectDataRaces = true;
                else if (part.rfind("default_order=", 0) == 0) memoryModelRule_.defaultOrder = part.substr(14);
            }
        }
    }
}

std::optional<GenomicTypeInfo> GenomicRuleEngine::getTypeInfo(TypeKind kind) const {
    auto it = typeDefinitions_.find(static_cast<int>(kind));
    if (it != typeDefinitions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<GenomicBinaryOpRule> GenomicRuleEngine::resolveBinaryOp(
    const std::string& op,
    TypeKind lhs,
    TypeKind rhs
) const {
    // 1. Exact match
    for (const auto& r : binaryOpRules_) {
        if (r.op == op && r.lhsKind == lhs && r.rhsKind == rhs) {
            return r;
        }
    }

    // 2. Promotion to common arithmetic type via genomic UAC rules
    auto commonType = getCommonType(lhs, rhs);
    if (commonType.has_value()) {
        for (const auto& r : binaryOpRules_) {
            if (r.op == op && r.lhsKind == *commonType && r.rhsKind == *commonType) {
                return r;
            }
        }
    }

    // 3. Generic standard conversion match
    for (const auto& r : binaryOpRules_) {
        if (r.op == op && canConvert(lhs, r.lhsKind) && canConvert(rhs, r.rhsKind)) {
            return r;
        }
    }

    return std::nullopt;
}

std::optional<GenomicUnaryOpRule> GenomicRuleEngine::resolveUnaryOp(
    const std::string& op,
    TypeKind operand,
    bool isPrefix
) const {
    for (const auto& r : unaryOpRules_) {
        if (r.op == op && r.operandKind == operand && r.isPrefix == isPrefix) {
            return r;
        }
    }
    return std::nullopt;
}

std::optional<TypeKind> GenomicRuleEngine::getCommonType(TypeKind lhs, TypeKind rhs) const {
    if (lhs == rhs) return lhs;

    for (const auto& r : arithmeticConversionRules_) {
        if ((r.lhsKind == lhs && r.rhsKind == rhs) ||
            (r.lhsKind == rhs && r.rhsKind == lhs)) {
            return r.commonKind;
        }
    }

    // Fallback: if one can convert to the other with low cost
    if (canConvert(lhs, rhs) && getConversionCost(lhs, rhs) <= 2) return rhs;
    if (canConvert(rhs, lhs) && getConversionCost(rhs, lhs) <= 2) return lhs;

    return std::nullopt;
}

int GenomicRuleEngine::getConversionCost(TypeKind from, TypeKind to) const {
    if (from == to) return 0; // Identity

    for (const auto& r : conversionRules_) {
        if (r.fromKind == from && r.toKind == to) {
            return r.isImplicitAllowed ? r.conversionCost : 999;
        }
    }
    return 999; // Incompatible
}

bool GenomicRuleEngine::canConvert(TypeKind from, TypeKind to) const {
    if (from == to) return true;
    int cost = getConversionCost(from, to);
    return cost < 999;
}

bool GenomicRuleEngine::checkConceptSatisfaction(const std::string& conceptName, TypeKind targetType) const {
    auto it = constraintRules_.find(conceptName);
    if (it == constraintRules_.end()) {
        return false;
    }
    for (auto k : it->second.satisfiedTypes) {
        if (k == targetType) return true;
    }
    return false;
}

bool GenomicRuleEngine::subsumes(const std::string& moreConstrained, const std::string& lessConstrained) const {
    if (moreConstrained == lessConstrained) return true;

    auto it = constraintRules_.find(moreConstrained);
    if (it == constraintRules_.end()) {
        // Built-in concept subsumption hierarchy
        if (moreConstrained == "signed_integral" && lessConstrained == "integral") return true;
        if (moreConstrained == "unsigned_integral" && lessConstrained == "integral") return true;
        if (moreConstrained == "arithmetic" && (lessConstrained == "integral" || lessConstrained == "floating_point")) return true;
        if (moreConstrained == "SignedIntegral" && lessConstrained == "Integral") return true;
        if (moreConstrained == "Integral" && lessConstrained == "Any") return true;
        return false;
    }

    for (const auto& sub : it->second.subsumedConcepts) {
        if (sub == lessConstrained || subsumes(sub, lessConstrained)) {
            return true;
        }
    }
    return false;
}

IrOpcode GenomicRuleEngine::getLoweringOpcode(const std::string& construct, IrOpcode defaultOp) const {
    auto it = loweringRules_.find(construct);
    if (it != loweringRules_.end()) {
        return it->second;
    }
    return defaultOp;
}

size_t GenomicRuleEngine::getRuleCount() const noexcept {
    return typeDefinitions_.size() + binaryOpRules_.size() + unaryOpRules_.size() +
           conversionRules_.size() + arithmeticConversionRules_.size() +
           constraintRules_.size() + loweringRules_.size();
}

std::string GenomicRuleEngine::serializeTypeInfo(const GenomicTypeInfo& info) {
    return "type:" + info.name + ":" + std::to_string(info.sizeBytes) + ":" +
           std::to_string(info.alignment) + ":" + (info.isIntegral ? "1" : "0") + ":" +
           (info.isFloatingPoint ? "1" : "0") + ":" + (info.isSigned ? "1" : "0") + ":" +
           (info.isPointer ? "1" : "0") + ":" + (info.isReference ? "1" : "0") + ";";
}

std::string GenomicRuleEngine::serializeBinaryOpRule(const GenomicBinaryOpRule& rule) {
    std::string valCatStr = (rule.resultValueCategory == ValueCategory::LValue) ? "LValue" : "PRValue";
    return "binop:" + rule.op + ":" + typeKindToString(rule.lhsKind) + ":" +
           typeKindToString(rule.rhsKind) + ":" + typeKindToString(rule.resultKind) + ":" +
           valCatStr + ":" + irOpcodeToMnemonic(rule.loweringOpcode) + ":" +
           (rule.isConstexprLegal ? "1" : "0") + ";";
}

std::string GenomicRuleEngine::serializeUnaryOpRule(const GenomicUnaryOpRule& rule) {
    std::string valCatStr = (rule.resultValueCategory == ValueCategory::LValue) ? "LValue" : "PRValue";
    return "unop:" + rule.op + ":" + typeKindToString(rule.operandKind) + ":" +
           typeKindToString(rule.resultKind) + ":" + (rule.isPrefix ? "1" : "0") + ":" +
           valCatStr + ":" + irOpcodeToMnemonic(rule.loweringOpcode) + ":" +
           (rule.isConstexprLegal ? "1" : "0") + ";";
}

std::string GenomicRuleEngine::serializeConversionRule(const GenomicConversionRule& rule) {
    return "conv:" + typeKindToString(rule.fromKind) + ":" + typeKindToString(rule.toKind) + ":" +
           std::to_string(rule.conversionCost) + ":" + (rule.isImplicitAllowed ? "1" : "0") + ":" +
           rule.category + ";";
}

std::string GenomicRuleEngine::serializeArithmeticConversionRule(const GenomicArithmeticConversionRule& rule) {
    return "uac:" + typeKindToString(rule.lhsKind) + ":" + typeKindToString(rule.rhsKind) + ":" +
           typeKindToString(rule.commonKind) + ";";
}

std::string GenomicRuleEngine::serializeConstraintRule(const GenomicConstraintRule& rule) {
    std::string types;
    for (size_t i = 0; i < rule.satisfiedTypes.size(); ++i) {
        if (i > 0) types += ",";
        types += typeKindToString(rule.satisfiedTypes[i]);
    }
    std::string subs;
    for (size_t i = 0; i < rule.subsumedConcepts.size(); ++i) {
        if (i > 0) subs += ",";
        subs += rule.subsumedConcepts[i];
    }
    return "concept:" + rule.conceptName + ":" + types + ":" + subs + ";";
}

std::string GenomicRuleEngine::serializeLoweringRule(const GenomicLoweringRule& rule) {
    return "lowering:" + rule.construct + ":" + irOpcodeToMnemonic(rule.opcode) + ";";
}

} // namespace genomic
