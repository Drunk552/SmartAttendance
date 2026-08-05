/**
 * @file result.h
 * @brief 提供符合 C++17 的显式成功/失败返回类型。
 */

#ifndef SMART_ATTENDANCE_CORE_COMMON_RESULT_H
#define SMART_ATTENDANCE_CORE_COMMON_RESULT_H

#include <utility>
#include <variant>

namespace smart_attendance {

template <typename T, typename E>
class Result final {
public:
    static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool hasValue() const noexcept {
        return value_.index() == 0;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    const T& value() const& {
        return std::get<0>(value_);
    }

    T& value() & {
        return std::get<0>(value_);
    }

    T&& value() && {
        return std::get<0>(std::move(value_));
    }

    const E& error() const& {
        return std::get<1>(value_);
    }

private:
    template <typename U>
    Result(std::in_place_index_t<0>, U&& value)
        : value_(std::in_place_index<0>, std::forward<U>(value)) {}

    template <typename U>
    Result(std::in_place_index_t<1>, U&& error)
        : value_(std::in_place_index<1>, std::forward<U>(error)) {}

    std::variant<T, E> value_;
};

template <typename E>
class Result<void, E> final {
public:
    static Result success() {
        return Result(std::in_place_index<0>);
    }

    static Result failure(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool hasValue() const noexcept {
        return value_.index() == 0;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    const E& error() const& {
        return std::get<1>(value_);
    }

private:
    explicit Result(std::in_place_index_t<0>)
        : value_(std::in_place_index<0>) {}

    template <typename U>
    Result(std::in_place_index_t<1>, U&& error)
        : value_(std::in_place_index<1>, std::forward<U>(error)) {}

    std::variant<std::monostate, E> value_;
};

} // namespace smart_attendance

#endif // SMART_ATTENDANCE_CORE_COMMON_RESULT_H
