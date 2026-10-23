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
    Nullptr,
    Enum,
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

enum class ConversionRank : uint8_t {
    ExactMatch = 0,          // Identity, Lvalue-to-rvalue, Array-to-pointer, Function-to-pointer, Qualification adjustment
    Promotion = 1,           // Integral promotion, Floating-point promotion
    StandardConversion = 2,  // Integral/Floating/Pointer/Boolean conversion
    UserDefinedConversion = 3,// Converting constructor / conversion operator
    EllipsisConversion = 4,  // Variadic ...
    Incompatible = 255
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
    bool isForwardingRef{false}; // True when T&& is a template parameter rvalue reference
    bool isPackExpansion{false}; // True for Ts...
    bool isNonDeduced{false};    // True for std::type_identity_t<T> or nested non-deduced context
    std::string name;

    // Subtypes for composite kinds
    std::shared_ptr<CppType> pointeeType;             // Pointer / Reference / Array / Enum underlying type
    size_t arraySize{0};                              // Array (or non-type template value)
    std::shared_ptr<CppType> returnType;              // Function
    std::vector<std::shared_ptr<CppType>> paramTypes; // Function
    std::vector<std::shared_ptr<CppType>> templateArgs; // Template specialization args e.g. std::vector<T>
    std::vector<FieldDefinition> fields;              // Record

    CppType() = default;
    explicit CppType(TypeKind k, bool c = false, bool v = false, std::string n = "")
        : kind(k), isConst(c), isVolatile(v), name(std::move(n)) {}

    bool isInteger() const noexcept;
    bool isSignedInteger() const noexcept;
    bool isUnsignedInteger() const noexcept;
    bool isFloatingPoint() const noexcept;
    bool isArithmetic() const noexcept;
    bool isIntegralOrUnscopedEnum() const noexcept;
    bool isSwitchConditionType() const noexcept;
    bool isPointer() const noexcept;
    bool isReference() const noexcept;
    bool isLValueReference() const noexcept { return kind == TypeKind::LValueReference; }
    bool isRValueReference() const noexcept { return kind == TypeKind::RValueReference; }
    bool isVoid() const noexcept;
    bool isRecord() const noexcept { return kind == TypeKind::Record; }
    bool isEnum() const noexcept { return kind == TypeKind::Enum; }

    std::string toString() const;
    bool isEquivalentTo(const CppType& other) const;
    bool isSameUnqualified(const CppType& other) const;

    // Type transformations (C++20 <type_traits> semantics)
    std::shared_ptr<CppType> removeReference() const;
    std::shared_ptr<CppType> removeCV() const;
    std::shared_ptr<CppType> removeCVRef() const;
    std::shared_ptr<CppType> decay() const;
    std::shared_ptr<CppType> withCV(bool addConst, bool addVolatile = false) const;

    // Factory methods
    static std::shared_ptr<CppType> makeVoid();
    static std::shared_ptr<CppType> makeAuto();
    static std::shared_ptr<CppType> makeBool();
    static std::shared_ptr<CppType> makeChar();
    static std::shared_ptr<CppType> makeInt8();
    static std::shared_ptr<CppType> makeInt16();
    static std::shared_ptr<CppType> makeInt32();
    static std::shared_ptr<CppType> makeInt64();
    static std::shared_ptr<CppType> makeUInt8();
    static std::shared_ptr<CppType> makeUInt16();
    static std::shared_ptr<CppType> makeUInt32();
    static std::shared_ptr<CppType> makeUInt64();
    static std::shared_ptr<CppType> makeFloat32();
    static std::shared_ptr<CppType> makeFloat64();
    static std::shared_ptr<CppType> makeNullptr();
    static std::shared_ptr<CppType> makeEnum(const std::string& name, std::shared_ptr<CppType> underlying = nullptr);
    static std::shared_ptr<CppType> makePointer(std::shared_ptr<CppType> pointee, bool isConst = false);
    static std::shared_ptr<CppType> makeLValueRef(std::shared_ptr<CppType> target);
    static std::shared_ptr<CppType> makeRValueRef(std::shared_ptr<CppType> target, bool isForwarding = false);
    static std::shared_ptr<CppType> makeArray(std::shared_ptr<CppType> element, size_t size);
    static std::shared_ptr<CppType> makeFunction(std::shared_ptr<CppType> ret, std::vector<std::shared_ptr<CppType>> params);
    static std::shared_ptr<CppType> makeRecord(const std::string& name, std::vector<std::shared_ptr<CppType>> tArgs = {});
    static std::shared_ptr<CppType> makeTemplateParam(const std::string& name, bool isPack = false);
    static std::shared_ptr<CppType> collapseReference(std::shared_ptr<CppType> baseType, bool outerIsLValueRef);
};

struct ImplicitConversionSequence {
    ConversionRank rank{ConversionRank::Incompatible};
    bool isReferenceBinding{false};
    bool bindsRValueRefToRValue{false};
    bool bindsLValueRefToNonConst{false};
    bool requiresQualificationAdjustment{false}; // e.g. T& -> const T& or T* -> const T*
    bool requiresPromotionToWide{false};
    int subRankCost{0}; // Fine-grained tie-breaker within same rank

    bool isValid() const noexcept { return rank != ConversionRank::Incompatible; }
};

class ClassRegistry;
class GenomicRuleEngine;

class TypeSystem {
public:
    static bool canImplicitlyConvert(const CppType& from, const CppType& to);
    static std::shared_ptr<CppType> usualArithmeticConversion(const CppType& t1, const CppType& t2);
    static ImplicitConversionSequence computeImplicitConversionSequence(
        const CppType& argType,
        ValueCategory argValCat,
        const CppType& paramType,
        const GenomicRuleEngine* ruleEngine = nullptr,
        const ClassRegistry* classRegistry = nullptr
    );
    // Returns -1 if seqA is better than seqB, +1 if seqB is better than seqA, 0 if indistinguishable
    static int compareConversionSequences(
        const ImplicitConversionSequence& seqA,
        const ImplicitConversionSequence& seqB,
        const CppType& paramA,
        const CppType& paramB
    );
};

} // namespace genomic
