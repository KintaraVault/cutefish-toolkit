// SPDX-License-Identifier: BSD-3-Clause
#pragma once

namespace cutefish::ui {

class ColorProvider;

// Only layout and painting borrow a window's material palette. Input,
// settings and the global theme change notifications use the system palette.
class ColorProviderScope
{
public:
    explicit ColorProviderScope(const ColorProvider* provider) : m_saved(s_active) { s_active = provider; }
    ~ColorProviderScope() { s_active = m_saved; }
    ColorProviderScope(const ColorProviderScope&) = delete;
    ColorProviderScope& operator=(const ColorProviderScope&) = delete;

    static const ColorProvider* active() { return s_active; }

private:
    inline static thread_local const ColorProvider* s_active = nullptr;
    const ColorProvider* m_saved;
};

} // namespace cutefish::ui
