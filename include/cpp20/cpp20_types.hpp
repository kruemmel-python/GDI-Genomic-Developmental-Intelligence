#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace genomic {

enum class TypeKind : uint16_t {
    Void,
    Bool,
    Char,
    Char8,
    Char16,
    Char32,
    WChar,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float32,
    Float64,
    Pointer,
    LValueReference,
    RValueReference,
    Array,
    Function,
    Record, // struct / class
    Auto,
    TemplateParam
};

enum class ValueCategory : uint8_t {
    LValue,
    PRValue,
    XValue
};

class CppType;
struct FieldDefinition {
    std::string name;
    std::shared_ptr<CppType> type;
    size_t offset{0};
    bool isPublic{true};
};

class CppType {
public:
    TypeKind kind{TypeKind::Void};
    bool isConst{false};
    bool isVolatile{false};
    std::string name;

    // Subtypes for composite kinds
    std::shared_ptr<CppType> pointeeType;           // Pointer / Reference / Array
    size_t arraySize{0};                            // Array
    std::shared_ptr<CppType> returnType;            // Function
    std::vector<std::shared_ptr<CppType>> paramTypes; // Function
    std::vector<FieldDefinition> fields;            // Record

    CppType() = default;
    explicit CppType(TypeKind k, bool c = false, bool v = false, std::string n = "")
        : kind(k), isConst(c), isVolatile(v), name(std::move(n)) {}

    bool isInteger() const noexcept;
    bool isFloatingPoint() const noexcept;
    bool isArithmetic() const noexcept;
    bool isPointer() const noexcept;
    bool isReference() const noexcept;
    bool isVoid() const noexcept;

    std::string toString() const;
    bool isEquivalentTo(const CppType& other) const;

    // Factory methods
    static std::shared_ptr<CppType> makeVoid();
    static std::shared_ptr<CppType> makeBool();
    static std::shared_ptr<CppType> makeInt32();
    static std::shared_ptr<CppType> makeInt64();
    static std::shared_ptr<CppType> makeUInt64();
    static std::shared_ptr<CppType> makeFloat64();
    static std::shared_ptr<CppType> makePointer(std::shared_ptr<CppType> pointee, bool isConst = false);
    static std::shared_ptr<CppType> makeLValueRef(std::shared_ptr<CppType> target);
    static std::shared_ptr<CppType> makeRValueRef(std::shared_ptr<CppType> target);
    static std::shared_ptr<CppType> makeFunction(std::shared_ptr<CppType> ret, std::vector<std::shared_ptr<CppType>> params);
    static std::shared_ptr<CppType> makeRecord(const std::string& name);
};

class TypeSystem {
public:
    static bool canImplicitlyConvert(const CppType& from, const CppType& to);
    static std::shared_ptr<CppType> usualArithmeticConversion(const CppType& t1, const CppType& t2);
};

} // namespace genomic
