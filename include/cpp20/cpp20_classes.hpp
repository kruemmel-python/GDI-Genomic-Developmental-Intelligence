#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include "cpp20/cpp20_types.hpp"

namespace genomic {

enum class AccessSpecifier : uint8_t {
    Public,
    Protected,
    Private
};

struct BaseClassSpecifier {
    std::string baseClassName;
    AccessSpecifier access{AccessSpecifier::Public};
    bool isVirtual{false};
};

struct MethodDefinition {
    std::string name;
    std::shared_ptr<CppType> signature;
    AccessSpecifier access{AccessSpecifier::Public};
    bool isVirtual{false};
    bool isPureVirtual{false};
    bool isOverride{false};
    bool isFinal{false};
    bool isStatic{false};
    size_t vtableSlot{0};
};

struct ClassModel {
    std::string name;
    bool isPolymorphic{false};
    bool isAbstract{false};
    std::vector<BaseClassSpecifier> baseClasses;
    std::vector<FieldDefinition> memberVariables;
    std::vector<MethodDefinition> methods;
    std::vector<std::string> vtable; // list of method names in vtable order

    bool hasVirtualDestructor{false};
    bool hasDefaultConstructor{true};
    bool hasCopyConstructor{true};
    bool hasMoveConstructor{true};
    bool hasCopyAssignment{true};
    bool hasMoveAssignment{true};

    // Construction and destruction order tracking
    std::vector<std::string> getConstructionOrder() const;
    std::vector<std::string> getDestructionOrder() const;

    // Virtual dispatch lookup
    std::optional<std::string> dispatchVirtual(size_t slot) const;
};

class ClassRegistry {
public:
    ClassRegistry() = default;

    void registerClass(const ClassModel& model);
    const ClassModel* lookupClass(const std::string& name) const;
    bool isDerivedFrom(const std::string& derivedName, const std::string& baseName) const;
    bool checkAccess(const std::string& className, const std::string& memberName, AccessSpecifier context) const;

private:
    std::unordered_map<std::string, ClassModel> classes_;
};

} // namespace genomic
