#pragma once

#include <utility>
#include <tuple>
#include <cstddef>

// Wrapper for template templates
template <template <typename> typename Template>
struct Wrapper {
    template <typename T>
    using Type = Template<T>;
};

// TemplateSet that directly handles templates and wraps them internally
template <template <typename> typename... Templates>
struct TemplateSet {
    // Internal wrappers for templates
    using WrappedTemplates = std::tuple<Wrapper<Templates>...>;

    // Retrieve a template by index (returns the Wrapper for the template)
    template <size_t i>
    using Get = typename std::tuple_element_t<i, WrappedTemplates>;

    // Append another set of templates
    template <template <typename> typename... OtherTemplates>
    using Append = TemplateSet<Templates..., OtherTemplates...>;

    // Concatenate with another TemplateSet
    template <typename OtherSet>
    struct CatImpl;

    template <template <typename> typename... OtherTemplates>
    struct CatImpl<TemplateSet<OtherTemplates...>> {
        using Type = TemplateSet<Templates..., OtherTemplates...>;
    };

    template <typename OtherSet>
    using Cat = typename CatImpl<OtherSet>::Type;

    // Apply a function to each template in the set
    template <typename F>
    static constexpr void foreach(F func) {
        // Apply the function to each wrapped template
        (func.template operator()<Templates>(), ...);
    }
};
