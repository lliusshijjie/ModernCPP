#include "detail.hpp"
#include <algorithm>
#include <queue>

namespace assetforge {
namespace {
[[noreturn]] void report_cycle(const Catalog& catalog, const std::vector<Index>& remaining) {
    // Iterative DFS only needs the active stack to reconstruct a back edge.
    struct Frame {
        Index id;
        std::size_t next;
    };

    std::vector<Frame> stack;
    std::vector<unsigned char> color(remaining.size());

    for (Index root = 0; root < remaining.size(); ++root) {
        if (!remaining[root] || color[root]) {
            continue;
        }
        color[root] = 1;
        stack.push_back({root, 0});

        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto& deps = catalog.dependencies()[frame.id];
            if (frame.next == deps.size()) {
                color[frame.id] = 2;
                stack.pop_back();
                continue;
            }

            const auto edge = deps[frame.next++];
            const auto target = edge.target;
            if (!remaining[target] || color[target] == 2) {
                continue;
            }
            if (color[target] == 0) {
                color[target] = 1;
                stack.push_back({target, 0});
                continue;
            }

            auto begin = std::find_if(stack.begin(), stack.end(),
                                      [target](Frame f) { return f.id == target; });
            std::string message = "dependency cycle: ";
            for (auto it = begin; it != stack.end(); ++it) {
                message += catalog.assets()[it->id].id + " -> ";
            }
            message += catalog.assets()[target].id;
            throw Error(detail::at_line(edge.line, message));
        }
    }
    throw std::logic_error("cycle reconstruction failed");
}
} // namespace

std::vector<Index> detail::topological_order(const Catalog& catalog,
                                             const std::vector<bool>& included) {
    const auto& assets = catalog.assets();
    const auto& dependencies = catalog.dependencies();
    std::vector<Index> remaining(assets.size());
    std::vector<std::vector<Index>> dependents(assets.size());

    auto later = [&](Index lhs, Index rhs) { return assets[lhs].id > assets[rhs].id; };
    std::priority_queue<Index, std::vector<Index>, decltype(later)> ready(later);
    std::size_t count{};
    for (Index id = 0; id < assets.size(); ++id) {
        if (!included[id]) {
            continue;
        }

        ++count;
        remaining[id] = static_cast<Index>(dependencies[id].size());
        for (const auto& [target, line] : dependencies[id]) {
            dependents[target].push_back(id);
        }
        if (remaining[id] == 0) {
            ready.push(id);
        }
    }

    std::vector<Index> order;
    order.reserve(count);
    while (!ready.empty()) {
        auto id = ready.top();
        ready.pop();
        order.push_back(id);
        for (auto waiting : dependents[id]) {
            if (--remaining[waiting] == 0) {
                ready.push(waiting);
            }
        }
    }

    if (order.size() != count) {
        report_cycle(catalog, remaining);
    }
    return order;
}

BuildPlan plan(const Catalog& catalog, const Selection& selection) {
    auto pending = std::visit(
        detail::Overloaded{[&](const Profile& profile) { return catalog.profile(profile.name); },
                           [&](const Root& root) {
                               if (auto id = catalog.find(root.name)) {
                                   return std::vector<Index>{*id};
                               }
                               throw Error("unknown asset or alias: " + root.name);
                           }},
        selection);

    std::vector<bool> included(catalog.assets().size(), false);
    while (!pending.empty()) {
        auto id = pending.back();
        pending.pop_back();
        if (included[id]) {
            continue;
        }

        included[id] = true;
        for (const auto& edge : catalog.dependencies()[id]) {
            pending.push_back(edge.target);
        }
    }

    auto order = detail::topological_order(catalog, included);
    std::vector<Index> ordinals(catalog.assets().size());
    BuildPlan result;
    result.directory_ = catalog.directory();
    result.assets_.reserve(order.size());

    for (auto id : order) {
        auto ordinal = static_cast<Index>(result.assets_.size());
        ordinals[id] = ordinal;

        PlannedAsset entry{catalog.assets()[id], {}};
        for (const auto& edge : catalog.dependencies()[id]) {
            entry.dependencies.push_back(ordinals[edge.target]);
        }
        std::sort(entry.dependencies.begin(), entry.dependencies.end());
        result.assets_.push_back(std::move(entry));
    }

    for (const auto& [name, target] : catalog.aliases()) {
        if (included[target]) {
            result.aliases_.try_emplace(name, ordinals[target]);
        }
    }
    return result;
}
} // namespace assetforge
