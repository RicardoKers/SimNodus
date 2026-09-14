// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/parameter_validation.hpp"
#include "application/binding_validation.hpp"
#include "application/topology_validation_internal.hpp"
#include <algorithm>
#include <new>
#include <set>
#include <utility>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
struct Decimal {
    bool negative = false;
    std::string digits;
    int exponent = 0;
    std::string fixed() const
    {
        std::string result = negative ? "-" : "";
        if(exponent >= 0) return result + digits + (digits == "0" ? "" : std::string(static_cast<std::size_t>(exponent), '0'));
        const auto fractional = static_cast<std::size_t>(-exponent);
        if(digits.size() <= fractional) return result + "0." + std::string(fractional - digits.size(), '0') + digits;
        return result + digits.substr(0, digits.size() - fractional) + "." + digits.substr(digits.size() - fractional);
    }
};
int compare(const Decimal& left, const Decimal& right)
{
    const bool lzero = left.digits == "0", rzero = right.digits == "0";
    if(lzero && rzero) return 0;
    const bool lnegative = left.negative && !lzero, rnegative = right.negative && !rzero;
    if(lnegative != rnegative) return lnegative ? -1 : 1;
    int magnitude = 0;
    if(lzero || rzero) magnitude = lzero ? -1 : 1;
    else {
        const int lorder = static_cast<int>(left.digits.size()) + left.exponent;
        const int rorder = static_cast<int>(right.digits.size()) + right.exponent;
        if(lorder != rorder) magnitude = lorder < rorder ? -1 : 1;
        else {
            const auto width = std::max(left.digits.size(), right.digits.size());
            for(std::size_t i = 0; i < width; ++i) {
                const char l = i < left.digits.size() ? left.digits[i] : '0';
                const char r = i < right.digits.size() ? right.digits[i] : '0';
                if(l != r) { magnitude = l < r ? -1 : 1; break; }
            }
        }
    }
    return lnegative ? -magnitude : magnitude;
}
struct Declaration { std::string unit; Decimal low, high, fallback; Index fallback_token; };
struct Binding { std::string parameter; Decimal literal; Index token; };
struct Instance { std::string id, target; Index token; std::map<std::string, Binding> overrides; };
struct Definition {
    Index token;
    std::map<std::string, Declaration> parameters;
    std::vector<Instance> instances;
    std::size_t cost = 0;
    bool cost_ready = false;
};
class Resolver {
public:
    explicit Resolver(TopologyDeclaration topology) : topology_(std::move(topology)), source_(*topology_.syntax) {}
    BindingDeclaration bindings()
    {
        auto parameters = run();
        const auto root = object(0);
        std::size_t entries = 0, bound_symbols = 0, bound_models = 0;
        const auto records = [&](Index index, std::initializer_list<const char*> fields) {
            Fields result;
            for(const auto token : array(index)) {
                const auto record = exact(token, fields);
                const auto id = identifier(record.at("id"));
                name(record.at("name"));
                require(result.emplace(id, token).second, "id", record.at("id"));
                ++entries;
            }
            return result;
        };
        const auto symbols = records(root.at("symbols"), {"id", "name", "pins"});
        const auto models = records(root.at("models"), {"id", "name", "terminals", "parameters"});
        std::map<std::string, Fields> anchors, terminals, slots;
        for(const auto& [id, token] : symbols)
            anchors.emplace(id, records(object(token).at("pins"), {"id", "name"}));
        for(const auto& [id, token] : models) {
            const auto record = object(token);
            auto pins = records(record.at("terminals"), {"id", "name", "domain"});
            for(const auto& [pid, pin] : pins) {
                (void)pid;
                const auto domain = object(pin).at("domain");
                require(string(domain, "value") == "electrical", "value", domain);
            }
            terminals.emplace(id, std::move(pins));
            auto parameters_slots = records(record.at("parameters"), {"id", "name", "unit", "minimum", "maximum"});
            for(const auto& [pid, slot] : parameters_slots) {
                (void)pid;
                const auto value = object(slot);
                const auto unit = string(value.at("unit"), "unit");
                require(unit == "ohm" || unit == "F" || unit == "V" || unit == "s" || unit == "1", "unit", value.at("unit"));
                const auto low = number(value.at("minimum")), high = number(value.at("maximum"));
                require(compare(low, high) <= 0, "range", slot);
            }
            slots.emplace(id, std::move(parameters_slots));
        }
        const auto mapping = [&](Index index, const auto& from, const Fields& to) {
            const auto fields = object(index);
            require(fields.size() == from.size(), "mapping", index);
            for(const auto& [id, token] : fields) {
                (void)token;
                require(from.contains(id), "mapping", index);
            }
            std::set<std::string> targets;
            for(const auto& [id, token] : fields) { (void)id; targets.insert(identifier(token)); }
            require(targets.size() == fields.size() && targets.size() == to.size(), "mapping", index);
            for(const auto& id : targets) require(to.contains(id), "mapping", index);
            entries += fields.size();
            return fields;
        };
        for(const bool component : {true, false}) {
            for(const auto token : array(root.at(component ? "components" : "circuits"))) {
                const auto definition = object(token);
                const auto did = string(definition.at("id"), "id");
                Fields pins;
                for(const auto pin : array(definition.at(component ? "pins" : "ports")))
                    pins.emplace(string(object(pin).at("id"), "id"), pin);
                const auto symbol_token = definition.at("symbol");
                if(source_.tokens[symbol_token].kind != JsonKind::null) {
                    const auto binding = exact(symbol_token, {"definition", "pin_map"});
                    const auto id = identifier(binding.at("definition"));
                    require(symbols.contains(id), "reference", binding.at("definition"));
                    mapping(binding.at("pin_map"), pins, anchors.at(id));
                    ++entries;
                    ++bound_symbols;
                }
                if(!component) continue;
                const auto model_token = definition.at("model");
                if(source_.tokens[model_token].kind == JsonKind::null) continue;
                const auto binding = exact(model_token, {"definition", "pin_map", "parameter_map"});
                const auto id = identifier(binding.at("definition"));
                require(models.contains(id), "reference", binding.at("definition"));
                // Both logical and model terminals were checked as electrical.
                mapping(binding.at("pin_map"), pins, terminals.at(id));
                const auto& declarations = definitions_.at(did).parameters;
                const auto parameter_map = mapping(binding.at("parameter_map"), declarations, slots.at(id));
                for(const auto& [pid, value] : parameter_map) {
                    const auto& declaration = declarations.at(pid);
                    const auto slot = object(slots.at(id).at(string(value, "id")));
                    require(declaration.unit == string(slot.at("unit"), "unit"), "unit", value);
                    require(compare(number(slot.at("minimum")), declaration.low) <= 0
                        && compare(declaration.high, number(slot.at("maximum"))) <= 0, "range", value);
                }
                ++entries;
                ++bound_models;
            }
        }
        require(topology_.declared_entities + extra_ + entries <= 4096, "budget", 0);
        return {std::move(parameters), entries, bound_symbols, bound_models, false};
    }
    ParameterSnapshot run()
    {
        const auto root = object(0);
        for(const char* collection : {"components", "circuits"}) {
            for(const auto index : array(root.at(collection))) {
                const auto fields = object(index);
                Definition definition{index, {}, {}, 0, false};
                for(const auto parameter : array(fields.at("parameters"))) {
                    const auto record = exact(parameter, {"id", "name", "unit", "default", "minimum", "maximum"});
                    const auto pid = identifier(record.at("id"));
                    name(record.at("name"));
                    const auto unit = string(record.at("unit"), "unit");
                    require(unit == "ohm" || unit == "F" || unit == "V" || unit == "s" || unit == "1", "unit", record.at("unit"));
                    auto low = number(record.at("minimum")), high = number(record.at("maximum")), fallback = number(record.at("default"));
                    require(compare(low, fallback) <= 0 && compare(fallback, high) <= 0, "range", parameter);
                    require(definition.parameters.emplace(pid, Declaration{unit, std::move(low), std::move(high), std::move(fallback), record.at("default")}).second, "id", record.at("id"));
                    ++extra_;
                }
                definitions_.emplace(string(fields.at("id"), "id"), std::move(definition));
            }
        }
        for(const auto circuit : array(root.at("circuits"))) {
            const auto fields = object(circuit);
            auto& definition = definitions_.at(string(fields.at("id"), "id"));
            for(const auto index : array(fields.at("instances"))) {
                const auto fields_instance = object(index);
                Instance instance{string(fields_instance.at("id"), "id"), string(fields_instance.at("definition"), "id"), index, {}};
                const auto& target = definitions_.at(instance.target).parameters;
                for(const auto& [pid, value] : object(fields_instance.at("overrides"))) {
                    // Object keys are decoded strings, with the same ID grammar.
                    require(valid_id(pid), "id", value);
                    const auto found = target.find(pid);
                    require(found != target.end(), "reference", value);
                    const auto& declaration = found->second;
                    const auto entries = object(value);
                    Binding binding{{}, {}, value};
                    if(entries.size() == 1 && entries.contains("parameter")) {
                        binding.parameter = identifier(entries.at("parameter"));
                        binding.token = entries.at("parameter");
                        const auto forwarded = definition.parameters.find(binding.parameter);
                        require(forwarded != definition.parameters.end(), "reference", binding.token);
                        const auto& source = forwarded->second;
                        require(source.unit == declaration.unit, "unit", binding.token);
                        require(compare(declaration.low, source.low) <= 0 && compare(source.high, declaration.high) <= 0, "range", binding.token);
                    } else {
                        const auto literal = exact(value, {"value", "unit"});
                        auto [unit, exponent] = quantity_unit(literal.at("unit"));
                        binding.literal = number(literal.at("value"));
                        binding.literal.exponent += exponent;
                        binding.token = literal.at("value");
                        require(unit == declaration.unit, "unit", literal.at("unit"));
                        require(compare(declaration.low, binding.literal) <= 0 && compare(binding.literal, declaration.high) <= 0, "range", value);
                    }
                    instance.overrides.emplace(pid, std::move(binding));
                    ++extra_;
                }
                definition.instances.push_back(std::move(instance));
            }
        }
        require(topology_.declared_entities + extra_ <= 4096, "budget", 0);
        for(auto& [id, definition] : definitions_) { (void)id; cost(definition); }
        const auto& root_definition = definitions_.at(topology_.root);
        ParameterSnapshot result{topology_, extra_, root_definition.cost, {}};
        result.instances.reserve(topology_.root_expanded_instances);
        std::map<std::string, Decimal> values;
        for(const auto& [pid, declaration] : root_definition.parameters) values.emplace(pid, declaration.fallback);
        visit(root_definition, {topology_.root}, values, result.instances);
        return result;
    }
private:
    TopologyDeclaration topology_;
    const DeclarationSyntax& source_;
    std::map<std::string, Definition> definitions_;
    std::size_t extra_ = 0;
    void require(bool condition, const char* code, Index index) const
    {
        if(!condition) throw ParameterError{code, source_.tokens[index].begin};
    }
    Fields object(Index index) const
    {
        const auto& token = source_.tokens[index];
        require(token.kind == JsonKind::object, "shape", index);
        Fields result;
        for(auto child = index + 1; child < token.next; child = source_.tokens[child + 1].next)
            result.emplace(source_.tokens[child].decoded, child + 1);
        return result;
    }
    Fields exact(Index index, std::initializer_list<const char*> expected) const
    {
        auto fields = object(index);
        require(fields.size() == expected.size(), "shape", index);
        for(const auto key : expected) require(fields.contains(key), "shape", index);
        return fields;
    }
    std::vector<Index> array(Index index) const
    {
        const auto& token = source_.tokens[index];
        require(token.kind == JsonKind::array, "shape", index);
        std::vector<Index> result;
        for(auto child = index + 1; child < token.next; child = source_.tokens[child].next) result.push_back(child);
        return result;
    }
    std::string string(Index index, const char* code) const
    {
        require(source_.tokens[index].kind == JsonKind::string, code, index);
        return source_.tokens[index].decoded;
    }
    static bool valid_id(const std::string& text)
    {
        return !text.empty() && text.size() <= 64 && text[0] >= 'a' && text[0] <= 'z'
            && std::all_of(text.begin(), text.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
    }
    std::string identifier(Index index) const
    {
        auto text = string(index, "id");
        require(valid_id(text), "id", index);
        return text;
    }
    void name(Index index) const
    {
        const auto text = string(index, "value");
        unsigned int scalar = 0;
        std::size_t length = 0;
        for(const auto byte : text) {
            const auto c = static_cast<unsigned char>(byte);
            if((c & 0xc0) != 0x80) {
                if(length) require(scalar >= 32 && !(scalar >= 127 && scalar <= 159), "value", index);
                ++length;
                scalar = c < 128 ? c : c < 224 ? c & 31 : c < 240 ? c & 15 : c & 7;
            } else scalar = (scalar << 6) | (c & 63);
        }
        require(length > 0 && length <= 80 && scalar >= 32 && !(scalar >= 127 && scalar <= 159), "value", index);
    }
    Decimal number(Index index) const
    {
        const auto raw = string(index, "quantity");
        require(!raw.empty() && raw.size() <= 64, "quantity", index);
        std::size_t pos = 0;
        const auto peek = [&]() { return pos < raw.size() ? raw[pos] : '\0'; };
        const auto digit = [](char c) { return c >= '0' && c <= '9'; };
        Decimal value;
        if(peek() == '-') { value.negative = true; ++pos; }
        if(peek() == '0') { value.digits += '0'; ++pos; }
        else {
            require(peek() >= '1' && peek() <= '9', "quantity", index);
            while(digit(peek())) value.digits += raw[pos++];
        }
        if(peek() == '.') {
            ++pos;
            require(digit(peek()), "quantity", index);
            while(digit(peek())) { value.digits += raw[pos++]; --value.exponent; }
        }
        require(value.digits.size() <= 32, "quantity", index);
        if(peek() == 'e') {
            ++pos;
            bool negative = false;
            if(peek() == '+' || peek() == '-') { negative = peek() == '-'; ++pos; }
            int exponent = 0;
            if(peek() == '0') ++pos;
            else {
                require(peek() >= '1' && peek() <= '9', "quantity", index);
                while(digit(peek())) {
                    exponent = exponent * 10 + (raw[pos++] - '0');
                    require(exponent <= 24, "quantity", index);
                }
            }
            value.exponent += negative ? -exponent : exponent;
        }
        require(pos == raw.size(), "quantity", index);
        const auto first = value.digits.find_first_not_of('0');
        value.digits = first == std::string::npos ? "0" : value.digits.substr(first);
        return value;
    }
    std::pair<std::string, int> quantity_unit(Index index) const
    {
        const auto unit = string(index, "unit");
        if(unit == "ohm" || unit == "F" || unit == "V" || unit == "s" || unit == "1") return {unit, 0};
        if(unit == "kohm") return {"ohm", 3};
        if(unit == "uF") return {"F", -6};
        if(unit == "nF") return {"F", -9};
        if(unit == "mV") return {"V", -3};
        if(unit == "ms") return {"s", -3};
        require(false, "unit", index);
        return {};
    }
    void cost(Definition& definition)
    {
        if(definition.cost_ready) return;
        // Structure already proved acyclic and at most eight definitions deep.
        for(const auto& instance : definition.instances) {
            auto& target = definitions_.at(instance.target);
            cost(target);
            definition.cost += 1 + target.parameters.size() + target.cost;
            require(definition.cost <= 16384, "budget", definition.token);
        }
        definition.cost_ready = true;
    }
    void visit(const Definition& definition, const std::vector<std::string>& path,
        const std::map<std::string, Decimal>& values, std::vector<ParameterOccurrence>& rows) const
    {
        for(const auto& instance : definition.instances) {
            const auto& target = definitions_.at(instance.target);
            auto child_path = path;
            child_path.push_back(instance.id);
            ParameterOccurrence row{child_path, instance.target, {}, source_.tokens[instance.token].begin};
            std::map<std::string, Decimal> effective;
            for(const auto& [pid, declaration] : target.parameters) {
                Decimal value = declaration.fallback;
                std::string origin = "default";
                auto token = declaration.fallback_token;
                const auto found = instance.overrides.find(pid);
                if(found != instance.overrides.end()) {
                    const auto& binding = found->second;
                    token = binding.token;
                    if(binding.parameter.empty()) { value = binding.literal; origin = "literal"; }
                    else { value = values.at(binding.parameter); origin = "containing-circuit:" + binding.parameter; }
                }
                effective.emplace(pid, value);
                row.parameters.emplace(pid, EffectiveParameter{value.fixed(), declaration.unit, origin, source_.tokens[token].begin});
            }
            rows.push_back(std::move(row));
            visit(target, child_path, effective, rows);
        }
    }
};
}
ParameterResult resolve_parameter_declaration(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return ParameterError{error->code == IngressErrorCode::memory ? "memory" : "input", error->offset};
        return std::make_shared<const ParameterSnapshot>(Resolver(detail::parameter_topology(std::get<0>(input))).run());
    } catch(const TopologyError& error) { return ParameterError{topology_error_name(error.code), error.offset}; }
    catch(const ParameterError& error) { return error; }
    catch(const std::bad_alloc&) { return ParameterError{"memory", 0}; }
}
BindingResult validate_binding_declaration(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return ParameterError{error->code == IngressErrorCode::memory ? "memory" : "input", error->offset};
        return std::make_shared<const BindingDeclaration>(Resolver(detail::binding_topology(std::get<0>(input))).bindings());
    } catch(const TopologyError& error) { return ParameterError{topology_error_name(error.code), error.offset}; }
    catch(const ParameterError& error) { return error; }
    catch(const std::bad_alloc&) { return ParameterError{"memory", 0}; }
}
}
