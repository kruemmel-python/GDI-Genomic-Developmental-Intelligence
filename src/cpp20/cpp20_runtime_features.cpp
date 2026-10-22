#include "cpp20/cpp20_runtime_features.hpp"

namespace genomic {

void ExceptionEngine::pushScope(const std::string& functionName) {
    callStack_.push_back(StackFrameScope{functionName, {}});
}

void ExceptionEngine::registerDestructor(std::function<void()> dtor) {
    if (!callStack_.empty()) {
        callStack_.back().destructorsToRun.push_back(dtor);
    }
}

void ExceptionEngine::popScope() {
    if (!callStack_.empty()) {
        // Run destructors in reverse order
        auto& dtors = callStack_.back().destructorsToRun;
        for (auto it = dtors.rbegin(); it != dtors.rend(); ++it) {
            if (*it) (*it)();
        }
        callStack_.pop_back();
    }
}

bool ExceptionEngine::unwindStackUntilCatch(
    const ExceptionRecord& ex,
    const std::vector<std::string>& catchTypes,
    std::vector<std::string>& outUnwoundDestructorCalls
) {
    bool caught = false;
    for (const auto& cType : catchTypes) {
        if (cType == "..." || cType == ex.typeName) {
            caught = true;
            break;
        }
    }

    // Unwind all frames until the handler
    while (!callStack_.empty()) {
        auto frame = callStack_.back();
        callStack_.pop_back();

        for (auto it = frame.destructorsToRun.rbegin(); it != frame.destructorsToRun.rend(); ++it) {
            if (*it) {
                (*it)();
                outUnwoundDestructorCalls.push_back("~" + frame.functionName + "Local()");
            }
        }
    }

    return caught;
}

bool ExceptionEngine::isNoexceptViolated(bool inNoexceptContext, bool isThrowing) const {
    return inNoexceptContext && isThrowing;
}

std::string RttiEngine::getTypeId(const std::string& className) const {
    return "type_info::" + className;
}

bool RttiEngine::canDynamicCast(const std::string& sourceClass, const std::string& targetClass) const {
    if (sourceClass == targetClass) return true;
    if (!registry_) return false;

    // Upcast or polymorphic downcast
    if (registry_->isDerivedFrom(sourceClass, targetClass)) {
        return true; // Upcast
    }
    if (registry_->isDerivedFrom(targetClass, sourceClass)) {
        auto srcModel = registry_->lookupClass(sourceClass);
        if (srcModel && srcModel->isPolymorphic) {
            return true; // Valid polymorphic downcast
        }
    }
    return false;
}

CoroutineHandle::CoroutineHandle(std::string name, std::shared_ptr<CppType> promiseType)
    : name_(std::move(name)), promiseType_(promiseType), status_(CoroutineStatus::Suspended) {}

void CoroutineHandle::resume() {
    if (status_ == CoroutineStatus::Completed) return;
    status_ = CoroutineStatus::Running;
    suspensionPoint_++;
}

void CoroutineHandle::destroy() {
    status_ = CoroutineStatus::Completed;
}

bool CoroutineHandle::done() const noexcept {
    return status_ == CoroutineStatus::Completed;
}

void CoroutineHandle::yieldValue(int64_t val) {
    lastYieldedValue_ = val;
    status_ = CoroutineStatus::Suspended;
}

void CoroutineHandle::returnVoid() {
    status_ = CoroutineStatus::Completed;
}

void CoroutineHandle::returnValue(int64_t val) {
    finalReturnValue_ = val;
    status_ = CoroutineStatus::Completed;
}

} // namespace genomic
