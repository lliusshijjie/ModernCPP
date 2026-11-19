#pragma once
#include "bundle.hpp"
#include <any>
#include <functional>
#include <type_traits>
#include <utility>

namespace assetforge {
class ReportRegistry {
public:
    // [18,29] Keep the statically paired collector and renderer behind a runtime name.
    template <class Collect, class Render>
    void add(std::string name, Collect collect, Render render) {
        using Value = std::decay_t<std::invoke_result_t<const Collect&, const BuildSummary&>>;
        static_assert(std::is_copy_constructible_v<Value>, "an any payload must be copyable");
        auto [it, added] = entries_.try_emplace(
            std::move(name),
            ExtraReport{[collect = std::move(collect)](const BuildSummary& summary) -> std::any {
                            return std::any(std::in_place_type<Value>,
                                            std::invoke(collect, summary));
                        },
                        [render = std::move(render)](const std::any& payload) {
                            auto value = std::any_cast<Value>(&payload);
                            if (!value) {
                                throw Error("extension report type mismatch");
                            }
                            return std::string(std::invoke(render, *value));
                        }});
        if (!added) {
            throw Error("duplicate report: " + it->first);
        }
    }

    bool contains(std::string_view name) const;
    std::string render(std::string_view name, const BuildSummary& summary) const;

private:
    struct ExtraReport {
        std::function<std::any(const BuildSummary&)> collect;
        std::function<std::string(const std::any&)> render;
    };

    std::map<std::string, ExtraReport, std::less<>> entries_;
};

ReportRegistry default_reports();
std::string decimal(Size value);
} // namespace assetforge
