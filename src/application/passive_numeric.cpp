// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/passive_numeric.hpp"
#include <algorithm>
#include <map>
#include <new>

namespace simnodus {
namespace {
// Private: only validated declaration numbers, resolved fixed values and
// recognized SPICE literals reach this decoder. It is not another ingress API.
struct Exact {
    bool negative = false;
    std::string digits;
    int exponent = 0;
    std::string fixed() const
    {
        if(digits == "0") return "0";
        const auto sign = negative ? "-" : "";
        if(exponent >= 0) return sign + digits + std::string(static_cast<std::size_t>(exponent), '0');
        const auto fraction = static_cast<std::size_t>(-exponent);
        if(fraction >= digits.size()) return sign + std::string("0.") + std::string(fraction - digits.size(), '0') + digits;
        return sign + digits.substr(0, digits.size() - fraction) + "." + digits.substr(digits.size() - fraction);
    }
};
Exact decode(std::string text)
{
    for(auto& c : text) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    Exact value;
    std::size_t i = 0;
    if(text[i] == '-') { value.negative = true; ++i; }
    bool fractional = false;
    for(; i < text.size(); ++i) {
        const auto c = text[i];
        if(c == '.') { fractional = true; continue; }
        if(c < '0' || c > '9') break;
        value.digits += c;
        if(fractional) --value.exponent;
    }
    if(i < text.size() && text[i] == 'e') {
        ++i;
        bool negative = false;
        if(text[i] == '+' || text[i] == '-') { negative = text[i] == '-'; ++i; }
        int exponent = 0;
        while(i < text.size() && text[i] >= '0' && text[i] <= '9') exponent = exponent * 10 + text[i++] - '0';
        value.exponent += negative ? -exponent : exponent;
    }
    static const std::map<std::string, int> scales{{"",0},{"t",12},{"g",9},{"meg",6},{"k",3},
        {"m",-3},{"u",-6},{"n",-9},{"p",-12},{"f",-15}};
    value.exponent += scales.at(text.substr(i));
    const auto first = value.digits.find_first_not_of('0');
    value.digits = first == std::string::npos ? "0" : value.digits.substr(first);
    if(value.digits == "0") { value.negative = false; value.exponent = 0; }
    while(value.digits.size() > 1 && value.digits.back() == '0') { value.digits.pop_back(); ++value.exponent; }
    return value;
}
int compare(const Exact& a, const Exact& b)
{
    if(a.negative != b.negative) return a.negative ? -1 : 1;
    int magnitude = 0;
    if(a.digits == "0" || b.digits == "0") magnitude = a.digits == b.digits ? 0 : a.digits == "0" ? -1 : 1;
    else {
        const auto order_a = static_cast<int>(a.digits.size()) + a.exponent;
        const auto order_b = static_cast<int>(b.digits.size()) + b.exponent;
        if(order_a != order_b) magnitude = order_a < order_b ? -1 : 1;
        else {
            const auto size = std::max(a.digits.size(), b.digits.size());
            const auto left = a.digits + std::string(size - a.digits.size(), '0');
            const auto right = b.digits + std::string(size - b.digits.size(), '0');
            magnitude = left == right ? 0 : left < right ? -1 : 1;
        }
    }
    return a.negative ? -magnitude : magnitude;
}
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
class Binder {
public:
    explicit Binder(std::shared_ptr<const PassiveInterface> matched)
        : syntax_(*matched->declaration->lock.syntax) { result_.interface = std::move(matched); }
    PassiveNumericBinding run()
    {
        const auto& interface = *result_.interface;
        const auto& source = interface.source->subcircuits[interface.model_index];
        const auto low = decode(interface.minimum), high = decode(interface.maximum), fallback = decode(source.default_literal);
        check(fallback, low, high, PassiveNumericStage::default_value, source.begin);
        result_.default_value = fallback.fixed();
        const auto topology = object(object(0).at("topology"));
        const auto components = topology.at("components");
        std::map<std::string, Fields> selected;
        for(auto i = components + 1; i < syntax_.tokens[components].next; i = syntax_.tokens[i].next) {
            const auto component = object(i);
            const auto model_index = component.at("model");
            if(syntax_.tokens[model_index].kind == JsonKind::null) continue;
            auto model = object(model_index);
            if(text(model.at("definition")) == interface.descriptor) selected.emplace(text(component.at("id")), std::move(model));
        }
        for(const auto& row : interface.declaration->topology.parameters.instances) {
            const auto found = selected.find(row.definition);
            if(found == selected.end()) continue;
            const auto& model = found->second;
            PassiveNumericOccurrence bound{row.path, row.definition, {}, {}, {}, row.source_offset};
            for(const auto& [parameter, target] : object(model.at("parameter_map")))
                if(text(target) == interface.parameter_id) bound.parameter = parameter;
            bound.effective = row.parameters.at(bound.parameter);
            check(decode(bound.effective.value), low, high, PassiveNumericStage::occurrence, bound.effective.source_offset);
            for(const auto& [pin, target] : object(model.at("pin_map")))
                for(Index position = 0; position < 2; ++position)
                    if(text(target) == interface.terminal_ids[position]) bound.pins[position] = pin;
            result_.occurrences.push_back(std::move(bound));
        }
        std::sort(result_.occurrences.begin(), result_.occurrences.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
        return std::move(result_);
    }
private:
    const DeclarationSyntax& syntax_;
    PassiveNumericBinding result_;
    static void check(const Exact& value, const Exact& low, const Exact& high, PassiveNumericStage stage, Index offset)
    {
        if(value.negative || value.digits == "0") throw PassiveNumericError{stage, "positive", offset};
        if(compare(value, low) < 0 || compare(value, high) > 0) throw PassiveNumericError{stage, "range", offset};
    }
    Fields object(Index index) const
    {
        Fields result;
        for(auto child = index + 1; child < syntax_.tokens[index].next; child = syntax_.tokens[child + 1].next)
            result.emplace(syntax_.tokens[child].decoded, child + 1);
        return result;
    }
    std::string text(Index index) const { return syntax_.tokens[index].decoded; }
};
}
PassiveNumericResult bind_passive_numeric(std::string_view bytes, std::string_view id, std::string_view source)
{
    try {
        const auto matched = match_passive_interface(bytes, id, source);
        if(const auto* error = std::get_if<PassiveInterfaceError>(&matched))
            return PassiveNumericError{PassiveNumericStage::interface, error->code, error->offset, error->stage};
        return std::make_shared<const PassiveNumericBinding>(Binder(std::get<0>(matched)).run());
    } catch(const PassiveNumericError& error) { return error; }
    catch(const std::bad_alloc&) { return PassiveNumericError{PassiveNumericStage::occurrence, "memory", 0}; }
}
}
