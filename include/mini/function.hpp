#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace mini {

template <class Signature>
class Function;

template <class R, class... Args>
class Function<R(Args...)> {
public:
    using result_type = R;

    Function() noexcept = default ;
    Function(std::nullptr_t) noexcept {}

    template <class F>
    requires (!std::is_same_v<std::remove_cvref_t<F>, Function>)
    Function(F&& callable)
    : callable_{new CallableModel<std::decay_t<F>>(std::forward<F>(callable))}/// cnay hoi confuse chut phai quay lai hoc lai
    {

    }

    Function(const Function& other) {
        if (other.callable_ != nullptr) {
            callable_ = other.callable_ -> clone();
        }
    }

    Function(Function&& other) noexcept {
        std::swap(callable_, other.callable_);
    }

    Function& operator=(Function other) {
        swap(other);
        return *this;
    }

    Function& operator=(std::nullptr_t) noexcept {
        delete callable_;
        callable_ = nullptr;
        return *this;
    }

    template <class F>
    requires (!std::is_same_v<std::remove_cvref_t<F>, Function>)
    Function& operator=(F&& callable) {
        Function temp{std::forward<F>(callable)};
        swap(temp);
        return *this;
    }

    ~Function() {
        delete callable_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return (callable_ != nullptr);
    }

    R operator()(Args... args) const {
        if (callable_ == nullptr) throw std::bad_function_call{};
        return callable_-> call(std::forward<Args>(args)...);
    }

    void swap(Function& other) noexcept {
        std::swap(callable_, other.callable_);
    }

private:
    // Stage 1: define the erased interface and a model that stores one
    // concrete callable type. The public Function type must never depend on F.
    struct CallableBase {
        virtual result_type call(Args&&... args) = 0;
        virtual ~CallableBase() {}
        virtual CallableBase* clone() const = 0;
    };

    template <class F>
    struct CallableModel : CallableBase{
        F function_;

        template<class G>
        CallableModel(G&& source)
            : function_{std::forward<G>(source)} {}

        result_type call(Args&&... args) override {
            return std::invoke(function_, std::forward<Args>(args)...);
        }

        CallableBase* clone() const override {
            return new CallableModel<F>(function_);
        }
    };

    CallableBase* callable_ = nullptr;
};

template <class R, class... Args>
void swap(Function<R(Args...)>& lhs,
          Function<R(Args...)>& rhs) noexcept {
    lhs.swap(rhs);
} // namespace mini
}
