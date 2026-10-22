#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_classes.hpp"

namespace genomic {

// --- Exception & Stack Unwinding Model ---
struct ExceptionRecord {
    std::string typeName;
    std::string message;
    int64_t errorCode{0};
};

struct StackFrameScope {
    std::string functionName;
    std::vector<std::function<void()>> destructorsToRun;
};

class ExceptionEngine {
public:
    ExceptionEngine() = default;

    void pushScope(const std::string& functionName);
    void registerDestructor(std::function<void()> dtor);
    void popScope();

    // Throws an exception, triggering unwinding until matching catch or abort
    bool unwindStackUntilCatch(const ExceptionRecord& ex,
                              const std::vector<std::string>& catchTypes,
                              std::vector<std::string>& outUnwoundDestructorCalls);

    bool isNoexceptViolated(bool inNoexceptContext, bool isThrowing) const;

private:
    std::vector<StackFrameScope> callStack_;
};

// --- RTTI & Dynamic Cast ---
class RttiEngine {
public:
    explicit RttiEngine(const ClassRegistry* registry) : registry_(registry) {}

    std::string getTypeId(const std::string& className) const;
    bool canDynamicCast(const std::string& sourceClass, const std::string& targetClass) const;

private:
    const ClassRegistry* registry_;
};

// --- Coroutine Frame & State Machine ---
enum class CoroutineStatus : uint8_t {
    NotStarted,
    Suspended,
    Running,
    Completed
};

class CoroutineHandle {
public:
    CoroutineHandle(std::string name, std::shared_ptr<CppType> promiseType);

    void resume();
    void destroy();
    bool done() const noexcept;
    CoroutineStatus status() const noexcept { return status_; }

    void yieldValue(int64_t val);
    void returnVoid();
    void returnValue(int64_t val);
    int64_t getYieldedValue() const noexcept { return lastYieldedValue_; }

private:
    std::string name_;
    std::shared_ptr<CppType> promiseType_;
    CoroutineStatus status_{CoroutineStatus::NotStarted};
    int64_t lastYieldedValue_{0};
    int64_t finalReturnValue_{0};
    size_t suspensionPoint_{0};
};

} // namespace genomic
