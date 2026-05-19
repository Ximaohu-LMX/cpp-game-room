#pragma once

namespace game {

/**
 * @brief 禁止拷贝基类。
 * @help 继承该类后，对象不能被拷贝构造或拷贝赋值。
 */
class Noncopyable {
protected:
    Noncopyable() = default;
    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

} // namespace game
