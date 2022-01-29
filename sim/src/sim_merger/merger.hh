#pragma once

#include "sim/primary_key.hh"
#include "src/sim_merger/primary_keys_from_jobs.hh"
#include "src/sim_merger/sim_merger.hh"
#include "src/sql_tables.hh"

#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace sim_merger {

// TODO: export it to simlib after updating it to the current version
class ProgressBar {
    std::string header_;
    size_t iter_ = 0;
    size_t iters_num_;
    size_t step_ = 0;

public:
    ProgressBar(std::string header, size_t iters_num, size_t step)
    : header_(std::move(header))
    , iters_num_(iters_num)
    , step_(step) {
        log();
    }

    void iter() {
        ++iter_;
        if (iter_ % step_ == 0 or iter_ == iters_num_) {
            log();
        }
    }

private:
    void log() {
        auto tmplog = stdlog("\033[2K\033[G", header_, ' ', iter_, " / ", iters_num_, " = ",
                100 * iter_ / iters_num_, "%");
        if (iter_ < iters_num_) {
            tmplog.flush_no_nl();
        }
    }
};

template <class CollA, class CollB, class FuncA, class FuncB, class Compare>
void merge(CollA&& collection_a, CollB&& collection_b, FuncA&& a_merge, FuncB&& b_merge,
        Compare&& is_left_lower) {
    auto a_beg = std::begin(collection_a);
    auto a_end = std::end(collection_a);
    auto b_beg = std::begin(collection_b);
    auto b_end = std::end(collection_b);
    while (a_beg != a_end or b_beg != b_end) {
        if (b_beg == b_end or (a_beg != a_end and is_left_lower(*a_beg, *b_beg))) {
            a_merge(*a_beg);
            ++a_beg;
        } else {
            b_merge(*b_beg);
            ++b_beg;
        }
    }
}

struct DefaultIdGetter {
    template <class T>
    auto& operator()(T&& x) const noexcept {
        return x.id;
    }
};

class MergerBase {
public:
    virtual const std::string& sql_table_name() noexcept = 0;

    virtual void save_merged() = 0;

    virtual void rollback_saving_merged_outside_database() noexcept {}

    virtual void run_after_saving_hooks() {}

    MergerBase() = default;
    MergerBase(const MergerBase&) = delete;
    MergerBase(MergerBase&&) = delete;
    MergerBase& operator=(const MergerBase&) = delete;
    MergerBase& operator=(MergerBase&&) = delete;
    virtual ~MergerBase() = default;
};

enum class IdKind { Main, Other };

template <class Record>
class Merger : public MergerBase {
    constexpr static bool debug = false;

    const std::string sql_table_name_;

protected:
    constexpr static auto primary_key = Record::primary_key;
    using PrimaryKeyType = typename decltype(Record::primary_key)::Type;

    PrimaryKeyType last_new_id_ = {};

    struct NewRecord {
        Record data;

        std::vector<PrimaryKeyType> main_ids = {};
        std::vector<PrimaryKeyType> other_ids = {};

        explicit NewRecord(const Record& new_data)
        : data(new_data) {}
    };

    struct RecordSet {
        friend class Merger;

        const IdKind kind;
        const std::string sql_table_name;
        const StringView sql_table_prefix;

    private:
        PrimaryKeysWithTime<PrimaryKeyType> ids;
        std::vector<Record> table = {};
        std::map<PrimaryKeyType, size_t> id_to_table_idx = {}; // Filled up after load
        std::map<PrimaryKeyType, PrimaryKeyType> id_to_new_id = {}; // Filled up during merge()

        void fill_id_to_table_idx() {
            for (size_t i = 0; i < table.size(); ++i) {
                id_to_table_idx.emplace(primary_key.get(table[i]), i);
            }
        }

        RecordSet(IdKind my_kind, std::string my_sql_tbl_name, StringView my_sql_tbl_prefix, PrimaryKeysWithTime<PrimaryKeyType> my_ids)
        : kind(my_kind)
        , sql_table_name(std::move(my_sql_tbl_name))
        , sql_table_prefix(my_sql_tbl_prefix)
        , ids(std::move(my_ids)) {}

    public:
        void add_record(Record record,
                std::chrono::system_clock::time_point closest_creation_time_approximation) {
            ids.add_id(primary_key.get(record), closest_creation_time_approximation);
            table.emplace_back(std::move(record));
        }

        [[nodiscard]] StringView sim_build() const {
            switch (kind) {
            case IdKind::Main: return main_sim_build;
            case IdKind::Other: return other_sim_build;
            }

            THROW("BUG");
        }
    };

    RecordSet main_;
    RecordSet other_;

    std::vector<NewRecord> new_table_;

    virtual void load(RecordSet& record_set) = 0;

    template <class Func>
    void merge(Func&& try_to_merge_into_an_existing_new_record) {
        constexpr bool takes_two_arguments =
                std::is_invocable_r_v<NewRecord*, Func, const Record&, IdKind>;
        static_assert(
                std::is_invocable_r_v<NewRecord*, Func, const Record&> or takes_two_arguments);

        auto try_merging_into_existing_new_record_wrapper =
                [&](const Record& x, IdKind kind) -> NewRecord* {
            if constexpr (takes_two_arguments) {
                return try_to_merge_into_an_existing_new_record(x, kind);
            } else {
                (void)kind; // Suppress GCC warning
                return try_to_merge_into_an_existing_new_record(x);
            }
        };

        using std::chrono::system_clock;

        auto process_id = [&](RecordSet& record_set, PrimaryKeyType id, system_clock::time_point tp) {
            auto log_added = [&](PrimaryKeyType new_id) {
                if constexpr (debug) {
                    stdlog("Added ", id_info(id, record_set.kind), " with new id: ", new_id,
                            "  ", mysql_date(system_clock::to_time_t(tp)));
                } else {
                    (void)new_id; // Suppress GCC warning
                }
            };

            auto it = record_set.id_to_table_idx.find(id);
            if (it == record_set.id_to_table_idx.end()) {
                // id has no corresponding record
                PrimaryKeyType new_id = pre_merge_record_id_to_post_merge_record_id(id);
                record_set.id_to_new_id.emplace(id, new_id);
                log_added(new_id);
                return;
            }

            const auto& record = record_set.table[it->second];
            NewRecord* new_record =
                    try_merging_into_existing_new_record_wrapper(record, record_set.kind);
            if (not new_record) {
                new_record = &new_table_.emplace_back(record);
                primary_key.set(new_record->data, pre_merge_record_id_to_post_merge_record_id(id));
            }

            record_set.id_to_new_id.emplace(id, primary_key.get(new_record->data));

            switch (record_set.kind) {
            case IdKind::Main: new_record->main_ids.emplace_back(id); break;
            case IdKind::Other: new_record->other_ids.emplace_back(id); break;
            }

            log_added(primary_key.get(new_record->data));
        };

        main_.ids.normalize_times();
        other_.ids.normalize_times();

        using IdsElem = std::pair<const PrimaryKeyType, system_clock::time_point>;
        sim_merger::merge(
                main_.ids.ids, other_.ids.ids,
                [&](const IdsElem& main_elem) {
                    process_id(main_, main_elem.first, main_elem.second);
                },
                [&](const IdsElem& other_elem) {
                    process_id(other_, other_elem.first, other_elem.second);
                },
                [&](const auto& main_elem, const auto& other_elem) {
                    auto& [main_id, main_tp] = main_elem;
                    auto& [other_id, other_tp] = other_elem;
                    return main_tp <= other_tp;
                });
    }

    virtual PrimaryKeyType pre_merge_record_id_to_post_merge_record_id(const PrimaryKeyType& record_id) {
        STACK_UNWINDING_MARK;
        (void)record_id;
        if constexpr (std::is_integral_v<PrimaryKeyType>) {
            return ++last_new_id_;
        } else {
            THROW("You have to overload this function with a sensible id "
                  "choosing");
        }
    }

    virtual void merge() = 0;

    Merger(StringView orig_sql_table_name, PrimaryKeysWithTime<PrimaryKeyType> main_ids,
            PrimaryKeysWithTime<PrimaryKeyType> other_ids)
    : sql_table_name_(orig_sql_table_name.to_string())
    , main_{IdKind::Main, concat_tostr(main_sim_table_prefix, orig_sql_table_name),
              main_sim_table_prefix, std::move(main_ids)}
    , other_{IdKind::Other, orig_sql_table_name.to_string(), "",
              std::move(other_ids)} {
        STACK_UNWINDING_MARK;

        assert(std::find(tables.begin(), tables.end(), orig_sql_table_name) != tables.end());
    }

    void initialize() {
        STACK_UNWINDING_MARK;

        load(main_);
        main_.fill_id_to_table_idx();

        load(other_);
        other_.fill_id_to_table_idx();

        merge();
    }

    static std::string id_info(PrimaryKeyType id, IdKind kind) {
        STACK_UNWINDING_MARK;
        switch (kind) {
        case IdKind::Main: return concat_tostr("(main id: ", id, ')');
        case IdKind::Other: return concat_tostr("(other id: ", id, ')');
        }

        THROW("BUG");
    }

    std::string id_info(const Record& record, IdKind kind) const {
        return id_info(primary_key.get(record), kind);
    }

    std::string id_info(const NewRecord& new_record) const {
        std::string res = concat_tostr("(new id: ", primary_key.get(new_record.data));
        if (not new_record.main_ids.empty()) {
            back_insert(res, " main ids:");
            for (auto& id : new_record.main_ids) {
                back_insert(res, ' ', id);
            }
        }
        if (not new_record.other_ids.empty()) {
            back_insert(res, " other ids:");
            for (auto& id : new_record.other_ids) {
                back_insert(res, ' ', id);
            }
        }

        back_insert(res, ')');
        return res;
    }

public:
    const std::string& sql_table_name() noexcept final { return sql_table_name_; }

    PrimaryKeyType new_main_id(PrimaryKeyType main_id) const {
        STACK_UNWINDING_MARK;
        auto it = main_.id_to_new_id.find(main_id);
        if (it == main_.id_to_new_id.end()) {
            THROW("Invalid main_id: ", main_id);
        }

        return it->second;
    }

    PrimaryKeyType new_other_id(PrimaryKeyType other_id) const {
        STACK_UNWINDING_MARK;
        auto it = other_.id_to_new_id.find(other_id);
        if (it == other_.id_to_new_id.end()) {
            THROW("Invalid other_id: ", other_id);
        }

        return it->second;
    }

    PrimaryKeyType new_id(PrimaryKeyType old_id, IdKind kind) const {
        STACK_UNWINDING_MARK;
        switch (kind) {
        case IdKind::Main: return new_main_id(old_id);
        case IdKind::Other: return new_other_id(old_id);
        }

        THROW("BUG");
    }
};

} // namespace sim_merger
