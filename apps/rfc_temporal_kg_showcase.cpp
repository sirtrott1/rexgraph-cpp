#include "rfc/rfc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using rfc::core::IdentityKey;
using rfc::numeric::Rational;
using rfc::numeric::RationalVector;

struct Entity {
    IdentityKey id{};
    std::string name;
    std::string category;
};

struct Fact {
    std::string id;
    IdentityKey subject{};
    std::string predicate;
    IdentityKey object{};
    long valid_from{};
    std::optional<long> valid_to;
    long asserted_at{};
    std::string source;
    Rational confidence{0};
};

struct Query {
    std::string id;
    long time{};
    IdentityKey subject{};
    std::string predicate;
    IdentityKey object{};
    bool expected{};
    std::string reason;
};

struct ContactRow {
    std::string id;
    IdentityKey source{};
    IdentityKey target{};
    long departure{};
    long arrival{};
    std::string label;
};

struct QueryResult {
    Query query;
    bool static_answer{};
    bool temporal_answer{};
    bool static_correct{};
    bool temporal_correct{};
    bool future_leakage_error{};
    bool stale_fact_error{};
};

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (const char ch : line) {
        if (ch == ',') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    fields.push_back(current);
    return fields;
}

long parse_long(const std::string& text) {
    std::size_t used = 0;
    const long value = std::stol(text, &used);
    if (used != text.size()) throw std::invalid_argument("invalid integer: " + text);
    return value;
}

std::uint64_t parse_u64(const std::string& text) {
    std::size_t used = 0;
    const auto value = std::stoull(text, &used);
    if (used != text.size()) throw std::invalid_argument("invalid unsigned integer: " + text);
    return value;
}

Rational parse_rational(const std::string& text) {
    const auto slash = text.find('/');
    if (slash == std::string::npos) return rfc::numeric::rational(parse_long(text));
    const auto numerator = parse_long(text.substr(0, slash));
    const auto denominator = parse_long(text.substr(slash + 1u));
    return rfc::numeric::rational(numerator, denominator);
}

std::vector<std::vector<std::string>> read_csv(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open " + path.string());
    std::vector<std::vector<std::string>> rows;
    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (first) {
            first = false;
            continue;
        }
        rows.push_back(split_csv_line(line));
    }
    return rows;
}

std::vector<Entity> load_entities(const std::filesystem::path& path) {
    std::vector<Entity> out;
    for (const auto& row : read_csv(path)) {
        if (row.size() != 3u) throw std::runtime_error("entities.csv column count");
        out.push_back(Entity{parse_u64(row[0]), row[1], row[2]});
    }
    return out;
}

std::vector<Fact> load_facts(const std::filesystem::path& path) {
    std::vector<Fact> out;
    for (const auto& row : read_csv(path)) {
        if (row.size() != 9u) throw std::runtime_error("facts.csv column count");
        std::optional<long> valid_to;
        if (row[5] != "INF") valid_to = parse_long(row[5]);
        out.push_back(Fact{row[0], parse_u64(row[1]), row[2], parse_u64(row[3]),
                           parse_long(row[4]), valid_to, parse_long(row[6]), row[7], parse_rational(row[8])});
    }
    return out;
}

std::vector<Query> load_queries(const std::filesystem::path& path) {
    std::vector<Query> out;
    for (const auto& row : read_csv(path)) {
        if (row.size() != 7u) throw std::runtime_error("queries.csv column count");
        out.push_back(Query{row[0], parse_long(row[1]), parse_u64(row[2]), row[3], parse_u64(row[4]),
                            parse_long(row[5]) != 0, row[6]});
    }
    return out;
}

std::vector<ContactRow> load_contacts(const std::filesystem::path& path) {
    std::vector<ContactRow> out;
    for (const auto& row : read_csv(path)) {
        if (row.size() != 6u) throw std::runtime_error("contacts.csv column count");
        out.push_back(ContactRow{row[0], parse_u64(row[1]), parse_u64(row[2]), parse_long(row[3]),
                                 parse_long(row[4]), row[5]});
    }
    return out;
}

bool fact_matches(const Fact& fact, const Query& query) {
    return fact.subject == query.subject && fact.predicate == query.predicate && fact.object == query.object;
}

bool fact_usable_at(const Fact& fact, long time) {
    if (fact.asserted_at > time) return false;
    if (fact.valid_from > time) return false;
    if (fact.valid_to && time >= *fact.valid_to) return false;
    return true;
}

bool static_answer(const std::vector<Fact>& facts, const Query& query) {
    return std::any_of(facts.begin(), facts.end(), [&](const Fact& fact) { return fact_matches(fact, query); });
}

bool temporal_answer(const std::vector<Fact>& facts, const Query& query) {
    return std::any_of(facts.begin(), facts.end(), [&](const Fact& fact) {
        return fact_matches(fact, query) && fact_usable_at(fact, query.time);
    });
}

std::string json_escape(const std::string& input) {
    std::ostringstream out;
    for (const unsigned char ch : input) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20u) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(ch)
                        << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(ch);
                }
        }
    }
    return out.str();
}

std::string rational_string(const Rational& value) { return rfc::numeric::str(value); }

std::string entity_name(const std::unordered_map<IdentityKey, Entity>& entities, IdentityKey id) {
    const auto it = entities.find(id);
    if (it == entities.end()) return std::to_string(id);
    return it->second.name;
}

std::string bool_json(bool value) { return value ? "true" : "false"; }

std::string join_names(const std::vector<std::string>& names, const std::string& sep) {
    std::ostringstream out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0u) out << sep;
        out << names[i];
    }
    return out.str();
}

std::vector<std::string> active_objects(const std::vector<Fact>& facts,
                                        IdentityKey subject,
                                        const std::string& predicate,
                                        long time,
                                        const std::unordered_map<IdentityKey, Entity>& entities,
                                        bool temporal) {
    std::set<std::string> values;
    for (const auto& fact : facts) {
        if (fact.subject != subject || fact.predicate != predicate) continue;
        if (temporal && !fact_usable_at(fact, time)) continue;
        values.insert(entity_name(entities, fact.object));
    }
    return std::vector<std::string>(values.begin(), values.end());
}

std::size_t static_single_value_conflicts(const std::vector<Fact>& facts) {
    std::map<std::pair<IdentityKey, std::string>, std::set<IdentityKey>> values;
    for (const auto& fact : facts) {
        if (fact.predicate != "demo:recommends") continue;
        values[{fact.subject, fact.predicate}].insert(fact.object);
    }
    std::size_t conflicts = 0;
    for (const auto& [key, objects] : values) {
        (void)key;
        if (objects.size() > 1u) ++conflicts;
    }
    return conflicts;
}

std::size_t temporal_single_value_conflicts(const std::vector<Fact>& facts, long first, long last) {
    std::size_t conflicts = 0;
    for (long time = first; time <= last; ++time) {
        std::map<std::pair<IdentityKey, std::string>, std::set<IdentityKey>> values;
        for (const auto& fact : facts) {
            if (fact.predicate != "demo:recommends" || !fact_usable_at(fact, time)) continue;
            values[{fact.subject, fact.predicate}].insert(fact.object);
        }
        for (const auto& [key, objects] : values) {
            (void)key;
            if (objects.size() > 1u) ++conflicts;
        }
    }
    return conflicts;
}

std::vector<std::string> route_labels(const std::vector<std::uint32_t>& relation_indices,
                                      const std::vector<ContactRow>& contacts) {
    std::vector<std::string> out;
    out.reserve(relation_indices.size());
    for (const auto relation : relation_indices) out.push_back(contacts.at(relation).label);
    return out;
}

std::vector<std::string> route_labels(const std::vector<std::size_t>& relation_indices,
                                      const std::vector<ContactRow>& contacts) {
    std::vector<std::string> out;
    out.reserve(relation_indices.size());
    for (const auto relation : relation_indices) out.push_back(contacts.at(relation).label);
    return out;
}

bool static_path_is_time_respecting(const std::vector<std::uint32_t>& relations,
                                    const std::vector<ContactRow>& contacts,
                                    IdentityKey source,
                                    IdentityKey target,
                                    long start_time) {
    IdentityKey current = source;
    long time = start_time;
    for (const auto relation : relations) {
        const auto& contact = contacts.at(relation);
        if (contact.source != current) return false;
        if (time > contact.departure) return false;
        current = contact.target;
        time = contact.arrival;
    }
    return current == target;
}

std::string vector_json(const RationalVector& values) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0u) out << ',';
        out << '"' << json_escape(rational_string(values[i])) << '"';
    }
    out << ']';
    return out.str();
}

std::string string_array_json(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0u) out << ',';
        out << '"' << json_escape(values[i]) << '"';
    }
    out << ']';
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path data_dir = "demo/temporal_kg/data";
        bool json = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--json") {
                json = true;
            } else if (arg == "--data-dir") {
                if (i + 1 >= argc) throw std::invalid_argument("--data-dir requires a path");
                data_dir = argv[++i];
            } else if (arg == "--help") {
                std::cout << "Usage: rfc_temporal_kg_showcase [--data-dir PATH] [--json]\n";
                return EXIT_SUCCESS;
            } else {
                throw std::invalid_argument("unknown argument: " + arg);
            }
        }

        const auto entities_list = load_entities(data_dir / "entities.csv");
        const auto facts = load_facts(data_dir / "facts.csv");
        const auto queries = load_queries(data_dir / "queries.csv");
        const auto contacts = load_contacts(data_dir / "contacts.csv");

        std::unordered_map<IdentityKey, Entity> entities;
        std::set<std::string> categories;
        for (const auto& entity : entities_list) {
            if (!entities.emplace(entity.id, entity).second) throw std::runtime_error("duplicate entity id");
            categories.insert(entity.category);
        }

        std::set<std::string> predicates;
        for (const auto& fact : facts) {
            if (!entities.contains(fact.subject) || !entities.contains(fact.object)) {
                throw std::runtime_error("fact endpoint missing from entity registry");
            }
            predicates.insert(fact.predicate);
        }

        rfc::core::RelationSignature signature;
        std::map<std::string, rfc::core::RelationTypeId> predicate_types;
        for (const auto& predicate : predicates) {
            predicate_types.emplace(predicate,
                signature.add_type(predicate, rfc::core::BoundaryPolicy::DeclaredExact, 2, 2));
        }
        rfc::core::RelationalComplexBuilder fact_builder(signature);
        for (const auto& fact : facts) {
            fact_builder.add_grade_one(predicate_types.at(fact.predicate), {
                {fact.subject, true, true, false, 1},
                {fact.object, true, false, false, 1}
            });
        }
        const auto fact_complex = fact_builder.freeze();

        rfc::database::ExactRelationalMemory memory(fact_complex);
        for (std::size_t i = 0; i < facts.size(); ++i) {
            memory.assert_exact({rfc::core::Grade{1}, static_cast<std::uint32_t>(i)},
                                {facts[i].confidence, {static_cast<rfc::database::ProvenanceId>(i + 1u)}});
        }
        std::size_t retained_records = 0;
        for (std::size_t i = 0; i < facts.size(); ++i) {
            if (memory.lookup({rfc::core::Grade{1}, static_cast<std::uint32_t>(i)})) ++retained_records;
        }

        RationalVector first_basis(facts.size(), Rational{0});
        if (!first_basis.empty()) first_basis[0] = 1;
        const auto first_boundary = fact_complex.boundary_apply(rfc::core::Grade{1}, first_basis);
        std::vector<std::pair<std::string, Rational>> boundary_terms;
        const auto& grade_zero = fact_complex.grade(rfc::core::Grade{0});
        for (std::size_t i = 0; i < first_boundary.size(); ++i) {
            if (first_boundary[i] != 0) {
                boundary_terms.emplace_back(entity_name(entities, grade_zero.identities.at(i)), first_boundary[i]);
            }
        }

        std::vector<QueryResult> results;
        results.reserve(queries.size());
        std::size_t static_correct_count = 0;
        std::size_t temporal_correct_count = 0;
        std::size_t future_leakage_errors = 0;
        std::size_t stale_fact_errors = 0;
        for (const auto& query : queries) {
            const bool s = static_answer(facts, query);
            const bool t = temporal_answer(facts, query);
            bool future = false;
            bool stale = false;
            if (s && !query.expected) {
                for (const auto& fact : facts) {
                    if (!fact_matches(fact, query)) continue;
                    if (fact.asserted_at > query.time || fact.valid_from > query.time) future = true;
                    if (fact.valid_to && query.time >= *fact.valid_to) stale = true;
                }
            }
            const bool sc = s == query.expected;
            const bool tc = t == query.expected;
            if (sc) ++static_correct_count;
            if (tc) ++temporal_correct_count;
            if (future) ++future_leakage_errors;
            if (stale) ++stale_fact_errors;
            results.push_back(QueryResult{query, s, t, sc, tc, future, stale});
        }

        const auto static_conflicts = static_single_value_conflicts(facts);
        const auto temporal_conflicts = temporal_single_value_conflicts(facts, 0, 14);
        const Rational relationship_richness(static_cast<long>(predicates.size()), static_cast<long>(facts.size()));
        const Rational attribute_richness(static_cast<long>(facts.size() * 5u), static_cast<long>(facts.size() * 5u));
        const Rational class_property_ratio(static_cast<long>(categories.size()), static_cast<long>(predicates.size()));
        const Rational interlinking_completeness(static_cast<long>(facts.size() * 2u), static_cast<long>(facts.size() * 2u));
        const Rational static_accuracy(static_cast<long>(static_correct_count), static_cast<long>(queries.size()));
        const Rational temporal_accuracy(static_cast<long>(temporal_correct_count), static_cast<long>(queries.size()));

        const auto guideline_day8_static = active_objects(facts, 5, "demo:recommends", 8, entities, false);
        const auto guideline_day8_temporal = active_objects(facts, 5, "demo:recommends", 8, entities, true);
        const auto guideline_day12_static = active_objects(facts, 5, "demo:recommends", 12, entities, false);
        const auto guideline_day12_temporal = active_objects(facts, 5, "demo:recommends", 12, entities, true);

        rfc::core::RelationSignature contact_signature;
        const auto contact_type = contact_signature.add_type("demo:scheduled_transition", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        rfc::core::RelationalComplexBuilder contact_builder(contact_signature);
        for (const auto& contact : contacts) {
            if (!entities.contains(contact.source) || !entities.contains(contact.target)) {
                throw std::runtime_error("contact endpoint missing from entity registry");
            }
            contact_builder.add_grade_one(contact_type, {
                {contact.source, true, true, false, 1},
                {contact.target, true, false, false, 1}
            });
        }
        const auto contact_complex = contact_builder.freeze();
        const auto static_view = rfc::view::project_pairwise_directed(contact_complex);
        const auto source_index = static_view.object_index(100);
        const auto target_index = static_view.object_index(104);
        const auto static_route = rfc::path::bfs_min_hops(static_view, source_index, target_index);

        std::vector<rfc::temporal::ScheduledContact> scheduled;
        scheduled.reserve(contacts.size());
        for (const auto& contact : contacts) {
            scheduled.push_back({static_view.object_index(contact.source), static_view.object_index(contact.target),
                                 Rational{contact.departure}, Rational{contact.arrival}});
        }
        const auto earliest = rfc::temporal::earliest_arrival(static_view.object_count(), scheduled, source_index, target_index, Rational{0});
        const auto fewest = rfc::temporal::fewest_contacts(static_view.object_count(), scheduled, source_index, target_index, Rational{0});
        const bool static_route_time_respecting = static_path_is_time_respecting(static_route.relations, contacts, 100, 104, 0);
        const auto static_route_names = route_labels(static_route.relations, contacts);
        const auto temporal_route_names = route_labels(earliest.contact_indices, contacts);
        const auto fewest_route_names = route_labels(fewest.contact_indices, contacts);

        const auto R0 = rfc::numeric::ExactMatrix::from_rows({
            {1, 0, 0},
            {0, Rational{1, 2}, 0},
            {0, Rational{1, 2}, 1}
        });
        const auto R1 = rfc::numeric::ExactMatrix::from_rows({
            {Rational{1, 2}, 0, 0},
            {Rational{1, 2}, 1, 0},
            {0, 0, 1}
        });
        const RationalVector initial{1, 0, 0};
        const RationalVector zero(3, Rational{0});
        const rfc::temporal::ChronologicalEvolution forward(initial, {R0, R1}, {zero, zero});
        const rfc::temporal::ChronologicalEvolution reversed(initial, {R1, R0}, {zero, zero});
        const rfc::temporal::OperationWord old_word({R0, R1});
        const rfc::temporal::OperationWord new_word({R1, R0});
        const auto I = rfc::numeric::ExactMatrix::identity(3);
        const auto defect = rfc::temporal::finite_word_defect(old_word, new_word, {I, I, I});
        const auto chronology_delta = defect.direct.apply(initial);

        if (json) {
            std::cout << "{\n";
            std::cout << "  \"dataset\": {\"entities\": " << entities_list.size()
                      << ", \"fact_relations\": " << facts.size()
                      << ", \"predicate_types\": " << predicates.size()
                      << ", \"entity_categories\": " << categories.size()
                      << ", \"retained_exact_records\": " << retained_records << "},\n";
            std::cout << "  \"boundary_example\": {\"relation\": \"" << json_escape(facts.front().id) << "\", \"terms\": [";
            for (std::size_t i = 0; i < boundary_terms.size(); ++i) {
                if (i != 0u) std::cout << ',';
                std::cout << "{\"entity\":\"" << json_escape(boundary_terms[i].first)
                          << "\",\"coefficient\":\"" << json_escape(rational_string(boundary_terms[i].second)) << "\"}";
            }
            std::cout << "]},\n";
            std::cout << "  \"benchmark\": {\n";
            std::cout << "    \"query_count\": " << queries.size() << ",\n";
            std::cout << "    \"static_correct\": " << static_correct_count << ",\n";
            std::cout << "    \"temporal_correct\": " << temporal_correct_count << ",\n";
            std::cout << "    \"static_accuracy\": \"" << rational_string(static_accuracy) << "\",\n";
            std::cout << "    \"temporal_accuracy\": \"" << rational_string(temporal_accuracy) << "\",\n";
            std::cout << "    \"static_future_leakage_errors\": " << future_leakage_errors << ",\n";
            std::cout << "    \"static_stale_fact_errors\": " << stale_fact_errors << ",\n";
            std::cout << "    \"temporal_future_leakage_errors\": 0,\n";
            std::cout << "    \"temporal_stale_fact_errors\": 0,\n";
            std::cout << "    \"static_single_value_conflicts\": " << static_conflicts << ",\n";
            std::cout << "    \"temporal_single_value_conflicts_days_0_14\": " << temporal_conflicts << ",\n";
            std::cout << "    \"amount_of_data\": " << facts.size() << ",\n";
            std::cout << "    \"relationship_richness_demo\": \"" << rational_string(relationship_richness) << "\",\n";
            std::cout << "    \"attribute_richness_demo\": \"" << rational_string(attribute_richness) << "\",\n";
            std::cout << "    \"class_property_instantiation_ratio_demo\": \"" << rational_string(class_property_ratio) << "\",\n";
            std::cout << "    \"interlinking_completeness_demo\": \"" << rational_string(interlinking_completeness) << "\"\n";
            std::cout << "  },\n";
            std::cout << "  \"queries\": [\n";
            for (std::size_t i = 0; i < results.size(); ++i) {
                const auto& result = results[i];
                std::cout << "    {\"id\":\"" << json_escape(result.query.id) << "\",\"time\":" << result.query.time
                          << ",\"subject\":\"" << json_escape(entity_name(entities, result.query.subject))
                          << "\",\"predicate\":\"" << json_escape(result.query.predicate)
                          << "\",\"object\":\"" << json_escape(entity_name(entities, result.query.object))
                          << "\",\"expected\":" << bool_json(result.query.expected)
                          << ",\"static\":" << bool_json(result.static_answer)
                          << ",\"temporal\":" << bool_json(result.temporal_answer)
                          << ",\"static_correct\":" << bool_json(result.static_correct)
                          << ",\"temporal_correct\":" << bool_json(result.temporal_correct)
                          << ",\"future_leakage_error\":" << bool_json(result.future_leakage_error)
                          << ",\"stale_fact_error\":" << bool_json(result.stale_fact_error)
                          << ",\"reason\":\"" << json_escape(result.query.reason) << "\"}";
                if (i + 1u != results.size()) std::cout << ',';
                std::cout << '\n';
            }
            std::cout << "  ],\n";
            std::cout << "  \"guideline_snapshots\": {\n";
            std::cout << "    \"day8\": {\"static\": " << string_array_json(guideline_day8_static)
                      << ", \"temporal\": " << string_array_json(guideline_day8_temporal) << "},\n";
            std::cout << "    \"day12\": {\"static\": " << string_array_json(guideline_day12_static)
                      << ", \"temporal\": " << string_array_json(guideline_day12_temporal) << "}\n";
            std::cout << "  },\n";
            std::cout << "  \"time_respecting_path\": {\n";
            std::cout << "    \"static_bfs_reachable\": " << bool_json(static_route.reachable) << ",\n";
            std::cout << "    \"static_bfs_hops\": \"" << rational_string(static_route.distance) << "\",\n";
            std::cout << "    \"static_bfs_path\": " << string_array_json(static_route_names) << ",\n";
            std::cout << "    \"static_bfs_is_time_respecting\": " << bool_json(static_route_time_respecting) << ",\n";
            std::cout << "    \"temporal_earliest_reachable\": " << bool_json(earliest.reachable) << ",\n";
            std::cout << "    \"temporal_earliest_arrival\": \"" << rational_string(earliest.arrival) << "\",\n";
            std::cout << "    \"temporal_earliest_contacts\": " << earliest.contacts << ",\n";
            std::cout << "    \"temporal_earliest_path\": " << string_array_json(temporal_route_names) << ",\n";
            std::cout << "    \"temporal_fewest_contacts\": " << fewest.contacts << ",\n";
            std::cout << "    \"temporal_fewest_arrival\": \"" << rational_string(fewest.arrival) << "\",\n";
            std::cout << "    \"temporal_fewest_path\": " << string_array_json(fewest_route_names) << "\n";
            std::cout << "  },\n";
            std::cout << "  \"chronology\": {\n";
            std::cout << "    \"forward_terminal\": " << vector_json(forward.terminal()) << ",\n";
            std::cout << "    \"reversed_terminal\": " << vector_json(reversed.terminal()) << ",\n";
            std::cout << "    \"finite_word_identity_exact\": " << bool_json(defect.exact_identity()) << ",\n";
            std::cout << "    \"reversed_minus_forward\": " << vector_json(chronology_delta) << "\n";
            std::cout << "  }\n";
            std::cout << "}\n";
            return EXIT_SUCCESS;
        }

        std::cout << "RFC TEMPORAL KG SHOWCASE\n";
        std::cout << "========================\n\n";
        std::cout << "1) Canonical relational memory\n";
        std::cout << "   entities=" << entities_list.size() << " retained_fact_relations=" << facts.size()
                  << " exact_records=" << retained_records << " predicate_types=" << predicates.size() << "\n";
        std::cout << "   first boundary: ";
        for (std::size_t i = 0; i < boundary_terms.size(); ++i) {
            if (i != 0u) std::cout << " + ";
            std::cout << boundary_terms[i].second << "*" << boundary_terms[i].first;
        }
        std::cout << "\n\n";

        std::cout << "2) Static KG vs temporal KG fact queries\n";
        std::cout << "   static accuracy=" << static_accuracy << " (" << static_correct_count << '/' << queries.size() << ")\n";
        std::cout << "   temporal accuracy=" << temporal_accuracy << " (" << temporal_correct_count << '/' << queries.size() << ")\n";
        std::cout << "   static future-leakage errors=" << future_leakage_errors
                  << " stale-fact errors=" << stale_fact_errors << "\n";
        std::cout << "   temporal future-leakage errors=0 stale-fact errors=0\n";
        for (const auto& result : results) {
            std::cout << "   " << result.query.id << " day " << result.query.time << ": "
                      << entity_name(entities, result.query.subject) << " --" << result.query.predicate << "--> "
                      << entity_name(entities, result.query.object) << " | expected=" << result.query.expected
                      << " static=" << result.static_answer << " temporal=" << result.temporal_answer << '\n';
        }
        std::cout << "\n";

        std::cout << "3) Evolving guideline snapshot\n";
        std::cout << "   day 8 static={" << join_names(guideline_day8_static, ", ") << "} temporal={"
                  << join_names(guideline_day8_temporal, ", ") << "}\n";
        std::cout << "   day 12 static={" << join_names(guideline_day12_static, ", ") << "} temporal={"
                  << join_names(guideline_day12_temporal, ", ") << "}\n";
        std::cout << "   static single-valued conflicts=" << static_conflicts
                  << " temporal conflicts(days 0..14)=" << temporal_conflicts << "\n\n";

        std::cout << "4) Time-respecting path\n";
        std::cout << "   static BFS: " << static_route.distance << " hops via [" << join_names(static_route_names, " -> ")
                  << "] time_respecting=" << static_route_time_respecting << "\n";
        std::cout << "   temporal earliest: arrival=" << earliest.arrival << " contacts=" << earliest.contacts
                  << " via [" << join_names(temporal_route_names, " -> ") << "]\n";
        std::cout << "   temporal fewest contacts: contacts=" << fewest.contacts << " arrival=" << fewest.arrival
                  << " via [" << join_names(fewest_route_names, " -> ") << "]\n\n";

        std::cout << "5) Exact chronology / noncommuting operation word\n";
        std::cout << "   forward terminal=";
        for (const auto& x : forward.terminal()) std::cout << ' ' << x;
        std::cout << "\n   reversed terminal=";
        for (const auto& x : reversed.terminal()) std::cout << ' ' << x;
        std::cout << "\n   reversed-forward defect=";
        for (const auto& x : chronology_delta) std::cout << ' ' << x;
        std::cout << "\n   finite telescoping identity exact=" << defect.exact_identity() << "\n\n";

        std::cout << "6) Transparent demo scorecard (not official NIH metric formulas)\n";
        std::cout << "   amount_of_data=" << facts.size() << '\n';
        std::cout << "   relationship_richness=" << relationship_richness << '\n';
        std::cout << "   attribute_richness=" << attribute_richness << '\n';
        std::cout << "   class_property_instantiation_ratio=" << class_property_ratio << '\n';
        std::cout << "   interlinking_completeness=" << interlinking_completeness << '\n';
        std::cout << "   logical_inconsistency_proxy static=" << static_conflicts << " temporal=" << temporal_conflicts << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
