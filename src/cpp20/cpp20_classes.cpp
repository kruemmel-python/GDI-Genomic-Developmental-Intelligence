#include "cpp20/cpp20_classes.hpp"
#include <algorithm>
#include <optional>

namespace genomic {

std::vector<std::string> ClassModel::getConstructionOrder() const {
    std::vector<std::string> order;
    // 1. Virtual bases first, then non-virtual bases in order of declaration
    for (const auto& base : baseClasses) {
        order.push_back("Base::" + base.baseClassName);
    }
    // 2. Member variables in order of declaration
    for (const auto& mem : memberVariables) {
        order.push_back("Member::" + mem.name);
    }
    // 3. Class body constructor
    order.push_back(name + "::Constructor");
    return order;
}

std::vector<std::string> ClassModel::getDestructionOrder() const {
    auto order = getConstructionOrder();
    // Destructors execute in exact reverse order of construction
    std::reverse(order.begin(), order.end());
    return order;
}

std::optional<std::string> ClassModel::dispatchVirtual(size_t slot) const {
    if (!vtable.empty() && slot < vtable.size()) {
        return vtable[slot];
    }
    // Dynamic fallback from declared virtual methods
    size_t currentSlot = 0;
    for (const auto& method : methods) {
        if (method.isVirtual) {
            if (currentSlot == slot) {
                return name + "::" + method.name;
            }
            currentSlot++;
        }
    }
    return std::nullopt;
}

void ClassRegistry::registerClass(const ClassModel& model) {
    auto m = model;
    // Build vtable if polymorphic
    size_t slot = 0;
    for (auto& method : m.methods) {
        if (method.isVirtual) {
            m.isPolymorphic = true;
            method.vtableSlot = slot++;
            m.vtable.push_back(m.name + "::" + method.name);
            if (method.isPureVirtual) {
                m.isAbstract = true;
            }
        }
    }
    classes_[m.name] = std::move(m);
}

const ClassModel* ClassRegistry::lookupClass(const std::string& name) const {
    auto it = classes_.find(name);
    if (it != classes_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ClassRegistry::isDerivedFrom(const std::string& derivedName, const std::string& baseName) const {
    if (derivedName == baseName) return true;
    auto cls = lookupClass(derivedName);
    if (!cls) return false;

    for (const auto& b : cls->baseClasses) {
        if (b.baseClassName == baseName || isDerivedFrom(b.baseClassName, baseName)) {
            return true;
        }
    }
    return false;
}

bool ClassRegistry::checkAccess(const std::string& className, const std::string& memberName, AccessSpecifier context) const {
    auto cls = lookupClass(className);
    if (!cls) return false;

    for (const auto& mem : cls->memberVariables) {
        if (mem.name == memberName) {
            if (mem.isPublic) return true;
            return context == AccessSpecifier::Private || context == AccessSpecifier::Protected;
        }
    }
    for (const auto& meth : cls->methods) {
        if (meth.name == memberName) {
            if (meth.access == AccessSpecifier::Public) return true;
            if (meth.access == AccessSpecifier::Protected) {
                return context == AccessSpecifier::Protected || context == AccessSpecifier::Private;
            }
            return context == AccessSpecifier::Private;
        }
    }
    return false;
}

} // namespace genomic
