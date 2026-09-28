#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::composite {

using AspectId = std::uint16_t;

enum class AspectRole : std::uint8_t {
    Existence = 0,
    Orientation = 1,
    RelationRole = 2,
    // Compatibility spelling only. A head is not a universal aspect; when it
    // exists it is one declared relation-role aspect in the signature.
    Head = RelationRole,
    Direction = 3,
    Type = 4,
    Visibility = 5,
    Activity = 6,
    Provenance = 7,
    Custom = 8
};

struct AspectDefinition {
    AspectId id{};
    std::string name;
    AspectRole role{AspectRole::Custom};
    bool default_value{false};
};

class AspectSchema {
public:
    AspectId add(std::string name, AspectRole role = AspectRole::Custom, bool default_value = false) {
        switch (role) {
            case AspectRole::Existence: case AspectRole::Orientation: case AspectRole::RelationRole:
            case AspectRole::Direction: case AspectRole::Type: case AspectRole::Visibility:
            case AspectRole::Activity: case AspectRole::Provenance: case AspectRole::Custom: break;
            default: throw std::invalid_argument("unknown aspect role");
        }
        if (name.empty()) throw std::invalid_argument("aspect name must not be empty");
        if (by_name_.contains(name)) throw std::invalid_argument("duplicate aspect name");
        if (definitions_.size() >= static_cast<std::size_t>(std::numeric_limits<AspectId>::max())) {
            throw std::overflow_error("too many aspects");
        }
        const auto id = static_cast<AspectId>(definitions_.size());
        definitions_.push_back(AspectDefinition{id, std::move(name), role, default_value});
        by_name_.emplace(definitions_.back().name, id);
        return id;
    }

    AspectId ensure(std::string name, AspectRole role = AspectRole::Custom, bool default_value = false) {
        if (const auto it = by_name_.find(name); it != by_name_.end()) {
            const auto& definition = definitions_.at(it->second);
            if (definition.role != role || definition.default_value != default_value) {
                throw std::invalid_argument("existing aspect has incompatible role/default");
            }
            return it->second;
        }
        return add(std::move(name), role, default_value);
    }

    [[nodiscard]] std::size_t size() const noexcept { return definitions_.size(); }
    [[nodiscard]] const AspectDefinition& at(AspectId id) const { return definitions_.at(id); }
    [[nodiscard]] AspectId id(std::string_view name) const {
        const auto it = by_name_.find(std::string(name));
        if (it == by_name_.end()) throw std::out_of_range("unknown aspect");
        return it->second;
    }
    [[nodiscard]] std::optional<AspectId> find_id(std::string_view name) const noexcept {
        const auto it = by_name_.find(std::string(name));
        if (it == by_name_.end()) return std::nullopt;
        return it->second;
    }
    [[nodiscard]] bool contains(std::string_view name) const { return by_name_.contains(std::string(name)); }

    [[nodiscard]] std::vector<AspectId> aspects_with_role(AspectRole role) const {
        std::vector<AspectId> result;
        for (const auto& definition : definitions_) {
            if (definition.role == role) result.push_back(definition.id);
        }
        return result;
    }

    [[nodiscard]] std::optional<AspectId> unique_aspect_with_role(AspectRole role) const {
        std::optional<AspectId> result;
        for (const auto& definition : definitions_) {
            if (definition.role != role) continue;
            if (result) throw std::logic_error("aspect role is not unique in schema");
            result = definition.id;
        }
        return result;
    }

    // The only universally declared composite-binary aspects in the canonical
    // source schema are signed-incidence existence and orientation. Relation
    // roles such as "head" are added by relation signatures that actually use
    // them.
    static AspectSchema signed_incidence() {
        AspectSchema schema;
        schema.add("exists", AspectRole::Existence, true);
        schema.add("orientation", AspectRole::Orientation, false);
        return schema;
    }

    static AspectSchema canonical() { return signed_incidence(); }

private:
    std::vector<AspectDefinition> definitions_;
    std::unordered_map<std::string, AspectId> by_name_;
};

} // namespace rfc::composite
