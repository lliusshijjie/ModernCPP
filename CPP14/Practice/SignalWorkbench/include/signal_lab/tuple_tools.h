#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace signal_lab {

// [03] index_sequence = integer_sequence<size_t, ...>。
// 初始化列表保证从左到右调用；首个 0 让空元组也形成合法数组。
template <class Tuple, class Visitor, std::size_t... I>
void visit_tuple_impl(Tuple&& row, Visitor& visit, std::index_sequence<I...>) {
    const int ordered[] = {
        0, (static_cast<void>(visit(std::get<I>(std::forward<Tuple>(row)))), 0)...
    };
    static_cast<void>(ordered);
}

template <class Tuple, class Visitor>
void visit_tuple(Tuple&& row, Visitor&& visit) {
    using Row = std::decay_t<Tuple>;
    visit_tuple_impl(std::forward<Tuple>(row), visit,
                     std::make_index_sequence<std::tuple_size<Row>::value>{});
}

} // namespace signal_lab
