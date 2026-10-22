#include "cpp20/cpp20_types.hpp"

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

bool CppType::isFloatingPoint() const noexcept {
    return kind == TypeKind::Float32 || kind == TypeKind::Float64;
}

bool CppType::isArithmetic() const noexcept {
    return isInteger() || isFloatingPoint();
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
            break;
        case TypeKind::Auto:
            s += "auto";
            break;
        case TypeKind::TemplateParam:
            s += name.empty() ? "typename T" : name;
            break;
    }
    return s;
}

bool CppType::isEquivalentTo(const CppType& other) const {
    if (kind != other.kind || isConst != other.isConst || isVolatile != other.isVolatile) {
        return false;
    }
    if (isPointer() || isReference() || kind == TypeKind::Array) {
        if (!pointeeType || !other.pointeeType) return false;
        return pointeeType->isEquivalentTo(*other.pointeeType);
    }
    if (kind == TypeKind::Record) {
        return name == other.name;
    }
    if (kind == TypeKind::Function) {
        if (!returnType->isEquivalentTo(*other.returnType)) return false;
        if (paramTypes.size() != other.paramTypes.size()) return false;
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (!paramTypes[i]->isEquivalentTo(*other.paramTypes[i])) return false;
        }
        return true;
    }
    return true;
}

std::shared_ptr<CppType> CppType::makeVoid() {
    return std::make_shared<CppType>(TypeKind::Void);
}

std::shared_ptr<CppType> CppType::makeBool() {
    return std::make_shared<CppType>(TypeKind::Bool);
}

std::shared_ptr<CppType> CppType::makeInt32() {
    return std::make_shared<CppType>(TypeKind::Int32);
}

std::shared_ptr<CppType> CppType::makeInt64() {
    return std::make_shared<CppType>(TypeKind::Int64);
}

std::shared_ptr<CppType> CppType::makeUInt64() {
    return std::make_shared<CppType>(TypeKind::UInt64);
}

std::shared_ptr<CppType> CppType::makeFloat64() {
    return std::make_shared<CppType>(TypeKind::Float64);
}

std::shared_ptr<CppType> CppType::makePointer(std::shared_ptr<CppType> pointee, bool isConst) {
    auto t = std::make_shared<CppType>(TypeKind::Pointer, isConst);
    t->pointeeType = pointee;
    return t;
}

std::shared_ptr<CppType> CppType::makeLValueRef(std::shared_ptr<CppType> target) {
    auto t = std::make_shared<CppType>(TypeKind::LValueReference);
    t->pointeeType = target;
    return t;
}

std::shared_ptr<CppType> CppType::makeRValueRef(std::shared_ptr<CppType> target) {
    auto t = std::make_shared<CppType>(TypeKind::RValueReference);
    t->pointeeType = target;
    return t;
}

std::shared_ptr<CppType> CppType::makeFunction(std::shared_ptr<CppType> ret,
                                               std::vector<std::shared_ptr<CppType>> params) {
    auto t = std::make_shared<CppType>(TypeKind::Function);
    t->returnType = ret;
    t->paramTypes = std::move(params);
    return t;
}

std::shared_ptr<CppType> CppType::makeRecord(const std::string& name) {
    return std::make_shared<CppType>(TypeKind::Record, false, false, name);
}

bool TypeSystem::canImplicitlyConvert(const CppType& from, const CppType& to) {
    if (from.isEquivalentTo(to)) return true;

    // Numeric promotions and conversions
    if (from.isArithmetic() && to.isArithmetic()) return true;

    // Pointer to const pointer
    if (from.isPointer() && to.isPointer()) {
        if (!to.isConst && from.isConst) return false;
        return true;
    }

    // Array to pointer decay
    if (from.kind == TypeKind::Array && to.isPointer()) {
        return from.pointeeType->isEquivalentTo(*to.pointeeType);
    }

    return false;
}

std::shared_ptr<CppType> TypeSystem::usualArithmeticConversion(const CppType& t1, const CppType& t2) {
    if (t1.kind == TypeKind::Float64 || t2.kind == TypeKind::Float64) {
        return CppType::makeFloat64();
    }
    if (t1.kind == TypeKind::UInt64 || t2.kind == TypeKind::UInt64) {
        return CppType::makeUInt64();
    }
    if (t1.kind == TypeKind::Int64 || t2.kind == TypeKind::Int64) {
        return CppType::makeInt64();
    }
    return CppType::makeInt32();
}

} // namespace genomic
