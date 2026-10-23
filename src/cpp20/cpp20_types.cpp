#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_classes.hpp"
#include "genome/genomic_rule_engine.hpp"

namespace genomic {

bool CppType::isInteger() const noexcept {
    switch (kind) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Char8:
        case TypeKind::Char16:
        case TypeKind::Char32:
        case TypeKind::WChar:
        case TypeKind::Int8:
        case TypeKind::Int16:
        case TypeKind::Int32:
        case TypeKind::Int64:
        case TypeKind::UInt8:
        case TypeKind::UInt16:
        case TypeKind::UInt32:
        case TypeKind::UInt64:
            return true;
        default:
            return false;
    }
}

bool CppType::isSignedInteger() const noexcept {
    switch (kind) {
        case TypeKind::Char:
        case TypeKind::WChar:
        case TypeKind::Int8:
        case TypeKind::Int16:
        case TypeKind::Int32:
        case TypeKind::Int64:
            return true;
        default:
            return false;
    }
}

bool CppType::isUnsignedInteger() const noexcept {
    switch (kind) {
        case TypeKind::Bool:
        case TypeKind::Char8:
        case TypeKind::Char16:
        case TypeKind::Char32:
        case TypeKind::UInt8:
        case TypeKind::UInt16:
        case TypeKind::UInt32:
        case TypeKind::UInt64:
            return true;
        default:
            return false;
    }
}

bool CppType::isFloatingPoint() const noexcept {
    return kind == TypeKind::Float32 || kind == TypeKind::Float64;
}

bool CppType::isArithmetic() const noexcept {
    return isInteger() || isFloatingPoint();
}

bool CppType::isIntegralOrUnscopedEnum() const noexcept {
    return isInteger() || kind == TypeKind::Enum;
}

bool CppType::isSwitchConditionType() const noexcept {
    if (isReference() && pointeeType) {
        return pointeeType->isSwitchConditionType();
    }
    return isInteger() || kind == TypeKind::Enum;
}

bool CppType::isPointer() const noexcept {
    return kind == TypeKind::Pointer;
}

bool CppType::isReference() const noexcept {
    return kind == TypeKind::LValueReference || kind == TypeKind::RValueReference;
}

bool CppType::isVoid() const noexcept {
    return kind == TypeKind::Void;
}

std::string CppType::toString() const {
    std::string s;
    if (isConst) s += "const ";
    if (isVolatile) s += "volatile ";

    switch (kind) {
        case TypeKind::Void: s += "void"; break;
        case TypeKind::Bool: s += "bool"; break;
        case TypeKind::Char: s += "char"; break;
        case TypeKind::Char8: s += "char8_t"; break;
        case TypeKind::Char16: s += "char16_t"; break;
        case TypeKind::Char32: s += "char32_t"; break;
        case TypeKind::WChar: s += "wchar_t"; break;
        case TypeKind::Int8: s += "int8_t"; break;
        case TypeKind::Int16: s += "int16_t"; break;
        case TypeKind::Int32: s += "int32_t"; break;
        case TypeKind::Int64: s += "int64_t"; break;
        case TypeKind::UInt8: s += "uint8_t"; break;
        case TypeKind::UInt16: s += "uint16_t"; break;
        case TypeKind::UInt32: s += "uint32_t"; break;
        case TypeKind::UInt64: s += "uint64_t"; break;
        case TypeKind::Float32: s += "float"; break;
        case TypeKind::Float64: s += "double"; break;
        case TypeKind::Nullptr: s += "std::nullptr_t"; break;
        case TypeKind::Enum:
            s += name.empty() ? "enum Anonymous" : name;
            break;
        case TypeKind::Pointer:
            s += (pointeeType ? pointeeType->toString() : "void") + "*";
            break;
        case TypeKind::LValueReference:
            s += (pointeeType ? pointeeType->toString() : "void") + "&";
            break;
        case TypeKind::RValueReference:
            s += (pointeeType ? pointeeType->toString() : "void") + "&&";
            break;
        case TypeKind::Array:
            s += (pointeeType ? pointeeType->toString() : "void") + "[" + std::to_string(arraySize) + "]";
            break;
        case TypeKind::Function:
            s += (returnType ? returnType->toString() : "void") + "(";
            for (size_t i = 0; i < paramTypes.size(); ++i) {
                if (i > 0) s += ", ";
                s += paramTypes[i]->toString();
            }
            s += ")";
            break;
        case TypeKind::Record:
            s += name.empty() ? "struct Anonymous" : name;
            if (!templateArgs.empty()) {
                s += "<";
                for (size_t i = 0; i < templateArgs.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += templateArgs[i] ? templateArgs[i]->toString() : "auto";
                }
                s += ">";
            }
            break;
        case TypeKind::Auto:
            s += "auto";
            break;
        case TypeKind::TemplateParam:
            s += name.empty() ? "typename T" : name;
            if (isPackExpansion) s += "...";
            break;
    }
    return s;
}

bool CppType::isSameUnqualified(const CppType& other) const {
    if (kind != other.kind) {
        return false;
    }
    if (isPointer() || isReference()) {
        if (!pointeeType || !other.pointeeType) return false;
        return pointeeType->isEquivalentTo(*other.pointeeType);
    }
    if (kind == TypeKind::Array) {
        if (arraySize != other.arraySize) return false;
        if (!pointeeType || !other.pointeeType) return false;
        return pointeeType->isEquivalentTo(*other.pointeeType);
    }
    if (kind == TypeKind::Record || kind == TypeKind::Enum || kind == TypeKind::TemplateParam) {
        if (name != other.name) return false;
        if (templateArgs.size() != other.templateArgs.size()) return false;
        for (size_t i = 0; i < templateArgs.size(); ++i) {
            if (!templateArgs[i] || !other.templateArgs[i]) return false;
            if (!templateArgs[i]->isEquivalentTo(*other.templateArgs[i])) return false;
        }
        return true;
    }
    if (kind == TypeKind::Function) {
        if (!returnType || !other.returnType || !returnType->isEquivalentTo(*other.returnType)) return false;
        if (paramTypes.size() != other.paramTypes.size()) return false;
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (!paramTypes[i]->isEquivalentTo(*other.paramTypes[i])) return false;
        }
        return true;
    }
    return true;
}

bool CppType::isEquivalentTo(const CppType& other) const {
    if (isConst != other.isConst || isVolatile != other.isVolatile) {
        return false;
    }
    return isSameUnqualified(other);
}

std::shared_ptr<CppType> CppType::removeReference() const {
    if (isReference() && pointeeType) {
        return std::make_shared<CppType>(*pointeeType);
    }
    return std::make_shared<CppType>(*this);
}

std::shared_ptr<CppType> CppType::removeCV() const {
    auto copy = std::make_shared<CppType>(*this);
    copy->isConst = false;
    copy->isVolatile = false;
    return copy;
}

std::shared_ptr<CppType> CppType::removeCVRef() const {
    return removeReference()->removeCV();
}

std::shared_ptr<CppType> CppType::decay() const {
    auto nonRef = removeReference();
    if (nonRef->kind == TypeKind::Array && nonRef->pointeeType) {
        return makePointer(nonRef->pointeeType);
    }
    if (nonRef->kind == TypeKind::Function) {
        return makePointer(nonRef);
    }
    return nonRef->removeCV();
}

std::shared_ptr<CppType> CppType::withCV(bool addConst, bool addVolatile) const {
    auto copy = std::make_shared<CppType>(*this);
    copy->isConst = copy->isConst || addConst;
    copy->isVolatile = copy->isVolatile || addVolatile;
    return copy;
}

std::shared_ptr<CppType> CppType::makeVoid() {
    return std::make_shared<CppType>(TypeKind::Void);
}

std::shared_ptr<CppType> CppType::makeAuto() {
    return std::make_shared<CppType>(TypeKind::Auto);
}

std::shared_ptr<CppType> CppType::makeBool() {
    return std::make_shared<CppType>(TypeKind::Bool);
}

std::shared_ptr<CppType> CppType::makeChar() {
    return std::make_shared<CppType>(TypeKind::Char);
}

std::shared_ptr<CppType> CppType::makeInt8() {
    return std::make_shared<CppType>(TypeKind::Int8);
}

std::shared_ptr<CppType> CppType::makeInt16() {
    return std::make_shared<CppType>(TypeKind::Int16);
}

std::shared_ptr<CppType> CppType::makeInt32() {
    return std::make_shared<CppType>(TypeKind::Int32);
}

std::shared_ptr<CppType> CppType::makeInt64() {
    return std::make_shared<CppType>(TypeKind::Int64);
}

std::shared_ptr<CppType> CppType::makeUInt8() {
    return std::make_shared<CppType>(TypeKind::UInt8);
}

std::shared_ptr<CppType> CppType::makeUInt16() {
    return std::make_shared<CppType>(TypeKind::UInt16);
}

std::shared_ptr<CppType> CppType::makeUInt32() {
    return std::make_shared<CppType>(TypeKind::UInt32);
}

std::shared_ptr<CppType> CppType::makeUInt64() {
    return std::make_shared<CppType>(TypeKind::UInt64);
}

std::shared_ptr<CppType> CppType::makeFloat32() {
    return std::make_shared<CppType>(TypeKind::Float32);
}

std::shared_ptr<CppType> CppType::makeFloat64() {
    return std::make_shared<CppType>(TypeKind::Float64);
}

std::shared_ptr<CppType> CppType::makeNullptr() {
    return std::make_shared<CppType>(TypeKind::Nullptr);
}

std::shared_ptr<CppType> CppType::makeEnum(const std::string& name, std::shared_ptr<CppType> underlying) {
    auto t = std::make_shared<CppType>(TypeKind::Enum, false, false, name);
    t->pointeeType = underlying ? std::move(underlying) : makeInt64();
    return t;
}

std::shared_ptr<CppType> CppType::makePointer(std::shared_ptr<CppType> pointee, bool isConst) {
    auto t = std::make_shared<CppType>(TypeKind::Pointer, isConst);
    t->pointeeType = std::move(pointee);
    return t;
}

std::shared_ptr<CppType> CppType::makeLValueRef(std::shared_ptr<CppType> target) {
    if (target && target->isReference()) {
        return collapseReference(target, true);
    }
    auto t = std::make_shared<CppType>(TypeKind::LValueReference);
    t->pointeeType = std::move(target);
    return t;
}

std::shared_ptr<CppType> CppType::makeRValueRef(std::shared_ptr<CppType> target, bool isForwarding) {
    if (target && target->isReference()) {
        return collapseReference(target, false);
    }
    auto t = std::make_shared<CppType>(TypeKind::RValueReference);
    t->isForwardingRef = isForwarding || (target && target->kind == TypeKind::TemplateParam && !target->isConst && !target->isVolatile);
    t->pointeeType = std::move(target);
    return t;
}

std::shared_ptr<CppType> CppType::makeArray(std::shared_ptr<CppType> element, size_t size) {
    auto t = std::make_shared<CppType>(TypeKind::Array);
    t->pointeeType = std::move(element);
    t->arraySize = size;
    return t;
}

std::shared_ptr<CppType> CppType::makeFunction(std::shared_ptr<CppType> ret,
                                               std::vector<std::shared_ptr<CppType>> params) {
    auto t = std::make_shared<CppType>(TypeKind::Function);
    t->returnType = std::move(ret);
    t->paramTypes = std::move(params);
    return t;
}

std::shared_ptr<CppType> CppType::makeRecord(const std::string& name, std::vector<std::shared_ptr<CppType>> tArgs) {
    auto t = std::make_shared<CppType>(TypeKind::Record, false, false, name);
    t->templateArgs = std::move(tArgs);
    return t;
}

std::shared_ptr<CppType> CppType::makeTemplateParam(const std::string& name, bool isPack) {
    auto t = std::make_shared<CppType>(TypeKind::TemplateParam, false, false, name);
    t->isPackExpansion = isPack;
    return t;
}

std::shared_ptr<CppType> CppType::collapseReference(std::shared_ptr<CppType> baseType, bool outerIsLValueRef) {
    if (!baseType) return makeVoid();
    // C++20 [dcl.ref]/6 reference collapsing rules:
    // T&  + &  -> T&
    // T&  + && -> T&
    // T&& + &  -> T&
    // T&& + && -> T&&
    if (baseType->kind == TypeKind::LValueReference) {
        return baseType; // Always collapses to T&
    }
    if (baseType->kind == TypeKind::RValueReference) {
        if (outerIsLValueRef) {
            auto collapsed = std::make_shared<CppType>(TypeKind::LValueReference);
            collapsed->pointeeType = baseType->pointeeType;
            return collapsed;
        }
        return baseType; // && + && -> &&
    }
    if (outerIsLValueRef) {
        auto ref = std::make_shared<CppType>(TypeKind::LValueReference);
        ref->pointeeType = std::move(baseType);
        return ref;
    }
    auto rref = std::make_shared<CppType>(TypeKind::RValueReference);
    rref->pointeeType = std::move(baseType);
    return rref;
}

static bool isIntegralPromotionPair(TypeKind from, TypeKind to) {
    if (to != TypeKind::Int32) return false;
    switch (from) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Char8:
        case TypeKind::Char16:
        case TypeKind::Int8:
        case TypeKind::Int16:
        case TypeKind::UInt8:
        case TypeKind::UInt16:
            return true;
        default:
            return false;
    }
}

static bool isFloatingPromotionPair(TypeKind from, TypeKind to) {
    return from == TypeKind::Float32 && to == TypeKind::Float64;
}

ImplicitConversionSequence TypeSystem::computeImplicitConversionSequence(
    const CppType& argType,
    ValueCategory argValCat,
    const CppType& paramType,
    const GenomicRuleEngine* ruleEngine,
    const ClassRegistry* classRegistry
) {
    ImplicitConversionSequence seq;

    // Unwrap reference on argument type if the argument expression had reference type
    const CppType* effectiveArg = &argType;
    ValueCategory effectiveCat = argValCat;
    if (argType.isLValueReference() && argType.pointeeType) {
        effectiveArg = argType.pointeeType.get();
        effectiveCat = ValueCategory::LValue;
    } else if (argType.isRValueReference() && argType.pointeeType) {
        effectiveArg = argType.pointeeType.get();
        // Named rvalue reference variables are LValues, xvalue expressions are XValues
        if (effectiveCat == ValueCategory::PRValue) {
            effectiveCat = ValueCategory::XValue;
        }
    }

    const bool isRValue = (effectiveCat == ValueCategory::PRValue || effectiveCat == ValueCategory::XValue);

    // 1. Reference binding (C++20 [over.ics.ref])
    if (paramType.isReference()) {
        if (!paramType.pointeeType) return seq;
        const CppType& target = *paramType.pointeeType;
        seq.isReferenceBinding = true;

        if (paramType.isLValueReference()) {
            // Non-const lvalue reference cannot bind to rvalue!
            if (!target.isConst && isRValue) {
                return seq; // Incompatible
            }
            // Cannot drop const/volatile qualifiers when binding reference
            if ((effectiveArg->isConst && !target.isConst) ||
                (effectiveArg->isVolatile && !target.isVolatile)) {
                return seq; // Incompatible
            }

            if (effectiveArg->isSameUnqualified(target)) {
                seq.rank = ConversionRank::ExactMatch;
                seq.bindsLValueRefToNonConst = !target.isConst && !isRValue;
                seq.requiresQualificationAdjustment = (target.isConst && !effectiveArg->isConst) ||
                                                      (target.isVolatile && !effectiveArg->isVolatile);
                seq.subRankCost = seq.requiresQualificationAdjustment ? 1 : 0;
                return seq;
            }

            // Derived-to-base reference binding
            if (effectiveArg->isRecord() && target.isRecord() && classRegistry &&
                classRegistry->isDerivedFrom(effectiveArg->name, target.name)) {
                seq.rank = ConversionRank::StandardConversion;
                seq.requiresQualificationAdjustment = (target.isConst && !effectiveArg->isConst);
                seq.subRankCost = 2;
                return seq;
            }

            // Const lvalue reference can bind to a materialized temporary converted to target
            if (target.isConst && !target.isVolatile) {
                auto valSeq = computeImplicitConversionSequence(*effectiveArg, effectiveCat, *target.removeCV(), ruleEngine, classRegistry);
                if (valSeq.isValid()) {
                    valSeq.isReferenceBinding = true;
                    valSeq.requiresQualificationAdjustment = true;
                    valSeq.subRankCost += 2;
                    return valSeq;
                }
            }
            return seq;
        }

        if (paramType.isRValueReference()) {
            // Rvalue reference cannot bind to lvalue!
            if (!isRValue) {
                return seq; // Incompatible
            }
            if ((effectiveArg->isConst && !target.isConst) ||
                (effectiveArg->isVolatile && !target.isVolatile)) {
                return seq;
            }

            if (effectiveArg->isSameUnqualified(target)) {
                seq.rank = ConversionRank::ExactMatch;
                seq.bindsRValueRefToRValue = true;
                seq.requiresQualificationAdjustment = (target.isConst && !effectiveArg->isConst) ||
                                                      (target.isVolatile && !effectiveArg->isVolatile);
                seq.subRankCost = seq.requiresQualificationAdjustment ? 1 : 0;
                return seq;
            }

            // Derived-to-base rvalue reference binding
            if (effectiveArg->isRecord() && target.isRecord() && classRegistry &&
                classRegistry->isDerivedFrom(effectiveArg->name, target.name)) {
                seq.rank = ConversionRank::StandardConversion;
                seq.bindsRValueRefToRValue = true;
                seq.subRankCost = 2;
                return seq;
            }

            // Rvalue reference can bind to a temporary produced by implicit conversion
            auto valSeq = computeImplicitConversionSequence(*effectiveArg, effectiveCat, *target.removeCV(), ruleEngine, classRegistry);
            if (valSeq.isValid()) {
                valSeq.isReferenceBinding = true;
                valSeq.bindsRValueRefToRValue = true;
                valSeq.subRankCost += 1;
                return valSeq;
            }
            return seq;
        }
    }

    // 2. Non-reference parameter: top-level cv-qualifiers on parameter type are ignored in C++20
    auto unqualParam = paramType.removeCV();
    auto unqualArg = effectiveArg->removeCV();

    // Exact match (Identity or Lvalue-to-rvalue)
    if (unqualArg->isEquivalentTo(*unqualParam)) {
        seq.rank = ConversionRank::ExactMatch;
        seq.subRankCost = 0;
        return seq;
    }

    // Array-to-pointer decay or Function-to-pointer decay (Exact Match rank in C++20 [over.ics.scs])
    if (unqualArg->kind == TypeKind::Array && unqualParam->isPointer() &&
        unqualArg->pointeeType && unqualParam->pointeeType) {
        if (unqualArg->pointeeType->isSameUnqualified(*unqualParam->pointeeType)) {
            if (unqualArg->pointeeType->isConst && !unqualParam->pointeeType->isConst) {
                return seq; // Cannot drop low-level const
            }
            seq.rank = ConversionRank::ExactMatch;
            seq.requiresQualificationAdjustment = (unqualParam->pointeeType->isConst && !unqualArg->pointeeType->isConst);
            seq.subRankCost = seq.requiresQualificationAdjustment ? 1 : 0;
            return seq;
        }
    }
    if (unqualArg->kind == TypeKind::Function && unqualParam->isPointer() && unqualParam->pointeeType &&
        unqualArg->isEquivalentTo(*unqualParam->pointeeType)) {
        seq.rank = ConversionRank::ExactMatch;
        return seq;
    }

    // Pointer conversions & Qualification adjustment
    if (unqualArg->isPointer() && unqualParam->isPointer() &&
        unqualArg->pointeeType && unqualParam->pointeeType) {
        const auto& srcPt = *unqualArg->pointeeType;
        const auto& dstPt = *unqualParam->pointeeType;

        // Qualification conversion: T* -> const T* (ExactMatch rank with qualification adjustment)
        if (srcPt.isSameUnqualified(dstPt)) {
            if ((srcPt.isConst && !dstPt.isConst) || (srcPt.isVolatile && !dstPt.isVolatile)) {
                return seq; // Illegal const-dropping
            }
            seq.rank = ConversionRank::ExactMatch;
            seq.requiresQualificationAdjustment = (dstPt.isConst && !srcPt.isConst) || (dstPt.isVolatile && !srcPt.isVolatile);
            seq.subRankCost = seq.requiresQualificationAdjustment ? 1 : 0;
            return seq;
        }

        // Pointer to void* (Standard conversion)
        if (dstPt.isVoid() && (!srcPt.isConst || dstPt.isConst) && (!srcPt.isVolatile || dstPt.isVolatile)) {
            seq.rank = ConversionRank::StandardConversion;
            seq.subRankCost = 3;
            return seq;
        }

        // Derived* -> Base* (Standard conversion)
        if (srcPt.isRecord() && dstPt.isRecord() && classRegistry &&
            classRegistry->isDerivedFrom(srcPt.name, dstPt.name)) {
            if ((srcPt.isConst && !dstPt.isConst) || (srcPt.isVolatile && !dstPt.isVolatile)) {
                return seq;
            }
            seq.rank = ConversionRank::StandardConversion;
            seq.requiresQualificationAdjustment = (dstPt.isConst && !srcPt.isConst);
            seq.subRankCost = 2;
            return seq;
        }
    }

    // nullptr_t -> any pointer
    if (unqualArg->kind == TypeKind::Nullptr && unqualParam->isPointer()) {
        seq.rank = ConversionRank::StandardConversion;
        seq.subRankCost = 2;
        return seq;
    }

    // Pointer -> bool conversion
    if ((unqualArg->isPointer() || unqualArg->kind == TypeKind::Nullptr) && unqualParam->kind == TypeKind::Bool) {
        seq.rank = ConversionRank::StandardConversion;
        seq.subRankCost = 4; // Boolean conversion is worse than other standard conversions per [over.ics.rank]
        return seq;
    }

    // Standalone ISO C++20 Promotion vs Standard Conversion rules
    if (isIntegralPromotionPair(unqualArg->kind, unqualParam->kind) ||
        isFloatingPromotionPair(unqualArg->kind, unqualParam->kind)) {
        seq.rank = ConversionRank::Promotion;
        seq.subRankCost = 1;
        return seq;
    }

    if (ruleEngine) {
        int cost = ruleEngine->getConversionCost(unqualArg->kind, unqualParam->kind);
        if (cost < 999) {
            if (cost == 0) seq.rank = ConversionRank::ExactMatch;
            else if (cost == 1) seq.rank = ConversionRank::Promotion;
            else if (cost == 2) seq.rank = ConversionRank::StandardConversion;
            else seq.rank = ConversionRank::UserDefinedConversion;
            seq.subRankCost = (unqualParam->kind == TypeKind::Bool && unqualArg->kind != TypeKind::Bool) ? 4 : cost;
            return seq;
        }
        // In C++20, integral conversions (e.g. int64_t <-> int32_t) and floating-point conversions
        // are always valid Standard Conversions unless conversion rules are explicitly restricted
        if (unqualArg->isInteger() && unqualParam->isInteger()) {
            seq.rank = ConversionRank::StandardConversion;
            seq.subRankCost = 2;
            return seq;
        }
        return seq;
    }

    if (unqualArg->isArithmetic() && unqualParam->isArithmetic()) {
        seq.rank = ConversionRank::StandardConversion;
        seq.subRankCost = (unqualParam->kind == TypeKind::Bool) ? 4 : 2;
        return seq;
    }

    return seq;
}

int TypeSystem::compareConversionSequences(
    const ImplicitConversionSequence& seqA,
    const ImplicitConversionSequence& seqB,
    const CppType& paramA,
    const CppType& paramB
) {
    if (!seqA.isValid() && !seqB.isValid()) return 0;
    if (seqA.isValid() && !seqB.isValid()) return -1;
    if (!seqA.isValid() && seqB.isValid()) return 1;

    // 1. Primary ConversionRank comparison (ExactMatch < Promotion < StandardConversion < UserDefined < Ellipsis)
    if (seqA.rank != seqB.rank) {
        return (static_cast<int>(seqA.rank) < static_cast<int>(seqB.rank)) ? -1 : 1;
    }

    // 2. Within StandardConversion: non-boolean conversion beats conversion to bool ([over.ics.rank]/4.2)
    if (seqA.rank == ConversionRank::StandardConversion && seqA.subRankCost != seqB.subRankCost) {
        return (seqA.subRankCost < seqB.subRankCost) ? -1 : 1;
    }

    // 3. Reference binding rules ([over.ics.rank]/3.2):
    // - Binding an rvalue reference to an rvalue beats binding an lvalue reference (e.g. const T&) to an rvalue
    if (seqA.isReferenceBinding && seqB.isReferenceBinding) {
        if (seqA.bindsRValueRefToRValue != seqB.bindsRValueRefToRValue) {
            return seqA.bindsRValueRefToRValue ? -1 : 1;
        }
        // Binding non-const T& to non-const lvalue beats binding const T& to non-const lvalue
        if (seqA.bindsLValueRefToNonConst != seqB.bindsLValueRefToNonConst) {
            return seqA.bindsLValueRefToNonConst ? -1 : 1;
        }
    }

    // 4. Qualification adjustment ([over.ics.rank]/3.2.6):
    // Less cv-qualification adjustment beats more cv-qualification adjustment (e.g. T* vs const T*)
    if (seqA.requiresQualificationAdjustment != seqB.requiresQualificationAdjustment) {
        // However, if one is pass-by-value `T` and the other is `const T&`, in C++ they are ambiguous for lvalue/rvalue!
        // Only disambiguate when both are references or both are pointers.
        if ((paramA.isReference() && paramB.isReference()) ||
            (paramA.isPointer() && paramB.isPointer())) {
            return !seqA.requiresQualificationAdjustment ? -1 : 1;
        }
    }

    return 0;
}

bool TypeSystem::canImplicitlyConvert(const CppType& from, const CppType& to) {
    if (from.isEquivalentTo(to)) return true;
    auto seq = computeImplicitConversionSequence(from, ValueCategory::LValue, to);
    if (seq.isValid()) return true;
    auto seqR = computeImplicitConversionSequence(from, ValueCategory::PRValue, to);
    return seqR.isValid();
}

std::shared_ptr<CppType> TypeSystem::usualArithmeticConversion(const CppType& t1, const CppType& t2) {
    if (t1.kind == TypeKind::Float64 || t2.kind == TypeKind::Float64) {
        return CppType::makeFloat64();
    }
    if (t1.kind == TypeKind::Float32 || t2.kind == TypeKind::Float32) {
        return CppType::makeFloat32();
    }
    if (t1.kind == TypeKind::UInt64 || t2.kind == TypeKind::UInt64) {
        return CppType::makeUInt64();
    }
    if (t1.kind == TypeKind::Int64 && t2.kind == TypeKind::UInt32) {
        return CppType::makeInt64();
    }
    if (t2.kind == TypeKind::Int64 && t1.kind == TypeKind::UInt32) {
        return CppType::makeInt64();
    }
    if (t1.kind == TypeKind::Int64 || t2.kind == TypeKind::Int64) {
        return CppType::makeInt64();
    }
    if (t1.kind == TypeKind::UInt32 || t2.kind == TypeKind::UInt32) {
        return CppType::makeUInt32();
    }
    return CppType::makeInt32();
}

} // namespace genomic
