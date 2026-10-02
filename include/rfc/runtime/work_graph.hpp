#pragma once

#include "rfc/runtime/resource_budget.hpp"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <thread>
#include <typeindex>

namespace rfc::runtime {
struct WorkKey {
    std::uint64_t id{},generation{};
    auto operator<=>(const WorkKey&) const=default;
};
struct WorkDemand {std::size_t workers{1},buffer_bytes{},result_bytes{};};
struct WorkAvailability {std::uint64_t source_namespace{},max_sequence{};};
inline void admit_work_availability(std::uint64_t source_namespace,std::uint64_t sequence,
                                   const std::optional<WorkAvailability>& cutoff) {
    if(cutoff && (!cutoff->source_namespace || cutoff->source_namespace!=source_namespace || sequence>cutoff->max_sequence))
        throw std::invalid_argument("work publication exceeds declared namespace availability");
}
class WorkValue {
public:
    template<class T> static WorkValue own(T value) {
        return WorkValue(typeid(T),std::make_shared<const T>(std::move(value)));
    }
    template<class T> std::shared_ptr<const T> as() const {
        if(type_!=typeid(T) || !value_)throw std::bad_cast{};
        return std::static_pointer_cast<const T>(value_);
    }
    bool same_owner(const WorkValue& other) const noexcept {return type_==other.type_ && value_==other.value_;}
private:
    WorkValue(std::type_index type,std::shared_ptr<const void> value):type_(type),value_(std::move(value)){}
    std::type_index type_;std::shared_ptr<const void> value_;
};
class WorkInputs {
public:
    const WorkValue& at(WorkKey key) const {
        for(const auto& entry:values_)if(entry.first==key)return entry.second;
        throw std::out_of_range("work input was not a declared dependency");
    }
private:
    std::vector<std::pair<WorkKey,WorkValue>> values_;
    friend class WorkGraph;
};
class WorkJob {
public:
    // Opt-in phase callbacks own the initial reservation and resize it at
    // declared stage boundaries. A busy upgrade fails rather than waiting while
    // holding resources needed by another pipeline. Legacy jobs stay delegated.
    template<class T,class Function> static WorkJob make_phased(WorkKey key,WorkDemand initial,WorkDemand peak,
        std::vector<WorkKey> dependencies,Function function) {
        if(!key.id || !key.generation || !initial.workers || !peak.workers || initial.workers>peak.workers ||
            initial.buffer_bytes>peak.buffer_bytes || initial.result_bytes!=peak.result_bytes || peak.result_bytes>peak.buffer_bytes)
            throw std::invalid_argument("invalid phased work declaration");
        auto definition=std::make_shared<Definition>();definition->key=key;definition->demand=initial;
        definition->dependencies=std::move(dependencies);definition->phase_peak=peak;
        definition->invoke_phased=[function=std::move(function)](const WorkInputs& inputs,CpuResourceCoordinator& resources,
            CpuResourceCoordinator::Reservation reservation,std::stop_token stop) {
            return WorkValue::own<T>(function(inputs,resources,std::move(reservation),stop));
        };
        return WorkJob(std::move(definition));
    }
    template<class T,class Function> static WorkJob make(WorkKey key,WorkDemand demand,
        std::vector<WorkKey> dependencies,Function function) {
        if(!key.id || !key.generation || !demand.workers)throw std::invalid_argument("work key/generation/workers must be positive");
        return WorkJob(key,demand,std::move(dependencies),
            [function=std::move(function)](const WorkInputs& inputs,CpuResourceCoordinator& assigned,std::stop_token stop) {
                return WorkValue::own<T>(function(inputs,assigned,stop));
            });
    }
    WorkKey key() const {return definition().key;}
    WorkDemand demand() const {return definition().demand;}
    WorkDemand peak_demand() const {return definition().phase_peak.value_or(definition().demand);}
    const std::vector<WorkKey>& dependencies() const & {return definition().dependencies;}
    const std::vector<WorkKey>& dependencies() const &&=delete;
private:
    struct Definition {
        WorkKey key;WorkDemand demand;std::vector<WorkKey> dependencies;
        std::function<WorkValue(const WorkInputs&,CpuResourceCoordinator&,std::stop_token)> invoke;
        std::optional<WorkDemand> phase_peak;
        std::function<WorkValue(const WorkInputs&,CpuResourceCoordinator&,CpuResourceCoordinator::Reservation,std::stop_token)> invoke_phased;
    };
    const Definition& definition() const {if(!definition_)throw std::invalid_argument("moved work job");return *definition_;}
    WorkJob(WorkKey key,WorkDemand demand,std::vector<WorkKey> dependencies,
        std::function<WorkValue(const WorkInputs&,CpuResourceCoordinator&,std::stop_token)> invoke)
        :definition_(std::make_shared<const Definition>(Definition{key,demand,std::move(dependencies),std::move(invoke),{}, {}})){}
    explicit WorkJob(std::shared_ptr<const Definition> definition):definition_(std::move(definition)){}
    std::shared_ptr<const Definition> definition_;
    friend class WorkGraph;
};
enum class WorkStatus {Queued,Running,Succeeded,Failed,Cancelled,Blocked};
inline bool work_terminal(WorkStatus status) noexcept {return status!=WorkStatus::Queued && status!=WorkStatus::Running;}
struct WorkReceipt {
    WorkKey key;WorkStatus status{WorkStatus::Queued};WorkDemand demand;
    std::uint64_t queue_ns{},execution_ns{};
};
struct WorkGraphLimits {std::size_t jobs{4096},edges{65536},in_flight{4},retained_result_bytes{std::size_t{1}<<30},depth{256};};
class WorkDependencyFailed:public std::runtime_error {
public:WorkDependencyFailed():std::runtime_error("a declared work dependency did not succeed"){}
};
namespace work_detail {
inline thread_local const void* executing_graph{};
using Clock=std::chrono::steady_clock;
inline std::uint64_t ns(Clock::time_point a,Clock::time_point b) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count());
}
struct Slot {
    WorkReceipt receipt;std::stop_source stop;std::optional<WorkValue> value;std::exception_ptr error;
};
struct State {
    std::vector<WorkJob> jobs;std::map<WorkKey,std::size_t> index;std::vector<Slot> slots;
    CpuResourceCoordinator resources;WorkGraphLimits limits;std::mutex mutex;std::condition_variable completed;
    std::stop_source stop;bool done{};std::size_t peak_jobs{},peak_workers{};
    State(std::vector<WorkJob> j,std::map<WorkKey,std::size_t> i,CpuResourceCoordinator& r,WorkGraphLimits l)
        :jobs(std::move(j)),index(std::move(i)),resources(r.share()),limits(l) {
        slots.reserve(jobs.size());for(const auto& job:jobs)slots.push_back({{job.key(),WorkStatus::Queued,job.demand(),0,0},{},{},{}});
    }
    void cancel_all() noexcept {
        stop.request_stop();for(auto& slot:slots)slot.stop.request_stop();resources.notify_waiters();
    }
};
struct Control {
    std::shared_ptr<State> state;std::jthread manager;std::mutex join_mutex;
    explicit Control(std::shared_ptr<State> s):state(std::move(s)){}
    ~Control() {
        state->cancel_all();
        // A callback dropping its own run cannot join the thread waiting for it.
        // The detached manager still owns State and joins every execution thread.
        if(executing_graph==state.get() && manager.joinable())manager.detach();
    }
    void join() {
        if(executing_graph==state.get())throw std::logic_error("work cannot wait for its own graph");
        std::lock_guard lock(join_mutex);if(manager.joinable())manager.join();
    }
};
} // namespace work_detail
class WorkCompletion {
public:
    WorkReceipt receipt() const {require_state();std::lock_guard lock(state_->mutex);return state_->slots.at(index_).receipt;}
    void cancel() const {require_state();state_->slots.at(index_).stop.request_stop();state_->resources.notify_waiters();}
    void wait() const {
        require_state();
        std::unique_lock lock(state_->mutex);
        if(work_detail::executing_graph==state_.get() && !work_terminal(state_->slots.at(index_).receipt.status))
            throw std::logic_error("work cannot wait for unfinished work in its own graph");
        state_->completed.wait(lock,[&]{return work_terminal(state_->slots.at(index_).receipt.status);});
    }
    template<class T> std::shared_ptr<const T> result() const {
        wait();std::lock_guard lock(state_->mutex);const auto& slot=state_->slots.at(index_);
        if(slot.receipt.status==WorkStatus::Cancelled)throw ExecutionCancelled{};
        if(slot.error)std::rethrow_exception(slot.error);
        if(slot.receipt.status!=WorkStatus::Succeeded || !slot.value)throw ExecutionCancelled{};
        return slot.value->as<T>();
    }
private:
    void require_state() const {if(!state_)throw std::invalid_argument("moved work completion");}
    WorkCompletion(std::shared_ptr<work_detail::State> state,std::size_t index):state_(std::move(state)),index_(index){}
    std::shared_ptr<work_detail::State> state_;std::size_t index_;
    friend class WorkRun;
    friend class WorkResultSlot;
};
// An explicitly selected prospective result. This never changes a native source.
// Receipt identity, as well as key/generation, prevents another run from binding.
class WorkResultSlot {
public:
    explicit WorkResultSlot(std::uint64_t id):id_(id) {if(!id)throw std::invalid_argument("zero result slot id");}
    void expect(WorkCompletion completion) {
        const auto key=completion.receipt().key;std::lock_guard lock(mutex_);
        if(key.id!=id_ || key.generation<=generation_)throw std::invalid_argument("result slot requires a newer declared generation");
        generation_=key.generation;expected_=std::move(completion);value_.reset();
    }
    bool accept(const WorkCompletion& completion) {
        completion.require_state();std::lock_guard lock(mutex_);
        if(!expected_ || expected_->state_!=completion.state_ || expected_->index_!=completion.index_)return false;
        std::lock_guard state_lock(completion.state_->mutex);const auto& slot=completion.state_->slots.at(completion.index_);
        if(slot.receipt.status!=WorkStatus::Succeeded || !slot.value)return false;
        value_=slot.value;return true;
    }
    template<class T> std::shared_ptr<const T> result() const {
        std::lock_guard lock(mutex_);if(!value_)throw std::logic_error("no accepted prospective work result");return value_->as<T>();
    }
private:
    const std::uint64_t id_;std::uint64_t generation_{};mutable std::mutex mutex_;
    std::optional<WorkCompletion> expected_;std::optional<WorkValue> value_;
};
class WorkRun {
public:
    WorkRun(const WorkRun&)=delete;WorkRun& operator=(const WorkRun&)=delete;
    WorkRun(WorkRun&&) noexcept=default;WorkRun& operator=(WorkRun&&) noexcept=default;
    WorkCompletion completion(WorkKey key) const {require_control();return {control_->state,control_->state->index.at(key)};}
    void cancel() const {require_control();control_->state->cancel_all();}
    void wait() const {require_control();control_->join();}
    std::pair<std::size_t,std::size_t> peak_jobs_workers() const {
        require_control();
        std::lock_guard lock(control_->state->mutex);return {control_->state->peak_jobs,control_->state->peak_workers};
    }
private:
    void require_control() const {if(!control_)throw std::invalid_argument("moved work run");}
    explicit WorkRun(std::shared_ptr<work_detail::Control> control):control_(std::move(control)){}
    std::shared_ptr<work_detail::Control> control_;
    friend class WorkGraph;
};
// Finite execution dependencies are scheduling declarations, not a chain complex
// or a temporal model. Inputs expose only successful declared predecessors.
class WorkGraph {
public:
    explicit WorkGraph(std::vector<WorkJob> jobs,WorkGraphLimits limits={})
        :jobs_(std::move(jobs)),limits_(limits) {
        if(jobs_.size()>limits.jobs || !limits.in_flight)throw std::length_error("work graph job/concurrency limit");
        std::size_t edges{},bytes{};
        for(std::size_t i=0;i<jobs_.size();++i) {
            if(!index_.emplace(jobs_[i].key(),i).second)throw std::invalid_argument("duplicate work key/generation");
            edges=machine::checked_size_add(edges,jobs_[i].dependencies().size());
            bytes=machine::checked_size_add(bytes,jobs_[i].demand().result_bytes);
        }
        if(edges>limits.edges || bytes>limits.retained_result_bytes)throw std::length_error("work graph edge/result budget");
        std::vector<std::size_t> depth(jobs_.size(),1),degree(jobs_.size());std::vector<std::vector<std::size_t>> children(jobs_.size());
        for(std::size_t i=0;i<jobs_.size();++i) {
            std::set<WorkKey> seen;
            for(auto key:jobs_[i].dependencies()) {
                if(!seen.insert(key).second)throw std::invalid_argument("duplicate work dependency");
                const auto found=index_.find(key);if(found==index_.end())throw std::invalid_argument("work dependency generation absent");
                ++degree[i];children[found->second].push_back(i);
            }
        }
        std::vector<std::size_t> ready;for(std::size_t i=0;i<degree.size();++i)if(!degree[i])ready.push_back(i);
        for(std::size_t i=0;i<ready.size();++i)for(auto child:children[ready[i]]){
            depth[child]=std::max(depth[child],machine::checked_size_add(depth[ready[i]],1));if(!--degree[child])ready.push_back(child);
        }
        for(auto d:depth)if(d>limits.depth)throw std::length_error("work graph depth budget");
        if(ready.size()!=jobs_.size())throw std::invalid_argument("cyclic work graph");
    }
    WorkRun submit(CpuResourceCoordinator& resources) const {
        std::size_t phase_results{},transient{};bool phased{};
        for(const auto& job:jobs_) {
            if(job.peak_demand().workers>resources.capacity() || job.peak_demand().buffer_bytes>resources.policy().declared_buffer_byte_limit)
                throw std::length_error("work demand exceeds shared resource capacity");
            if(job.definition_->phase_peak) {
                phased=true;phase_results=machine::checked_size_add(phase_results,job.peak_demand().result_bytes);
                transient=std::max(transient,job.peak_demand().buffer_bytes-job.peak_demand().result_bytes);
            } else transient=std::max(transient,job.demand().buffer_bytes);
        }
        // Completed phase results stay charged. Reject an own-graph retention
        // envelope that could prevent every remaining flat job from starting.
        if(phased && machine::checked_size_add(phase_results,transient)>resources.policy().declared_buffer_byte_limit)
            throw std::length_error("phased graph retained results/transient envelope exceeds cooperating bytes");
        auto state=std::make_shared<work_detail::State>(jobs_,index_,resources,limits_);
        auto control=std::make_shared<work_detail::Control>(state);
        control->manager=std::jthread([state]{dispatch(state);});return WorkRun(std::move(control));
    }
private:
    static void dispatch(const std::shared_ptr<work_detail::State>& state) noexcept {
        using namespace work_detail;
        const auto queued=Clock::now();std::vector<std::optional<std::jthread>> threads;
        try {
            threads.resize(state->jobs.size());
            for(;;) {
                const auto epoch=state->resources.change_epoch();std::size_t active{},terminal{};
                {
                    std::lock_guard lock(state->mutex);
                    for(std::size_t i=0;i<state->slots.size();++i) {
                        auto& slot=state->slots[i];
                        if(slot.receipt.status==WorkStatus::Queued && (state->stop.stop_requested() || slot.stop.stop_requested())) {
                            slot.receipt.status=WorkStatus::Cancelled;slot.error=std::make_exception_ptr(ExecutionCancelled{});
                        }
                        if(slot.receipt.status==WorkStatus::Running){++active;}
                        if(work_terminal(slot.receipt.status))++terminal;
                    }
                }
                for(std::size_t i=0;i<threads.size();++i) {
                    bool complete;{std::lock_guard lock(state->mutex);complete=work_terminal(state->slots[i].receipt.status);}
                    if(complete && threads[i]){threads[i]->join();threads[i].reset();}
                }
                if(terminal==state->jobs.size())break;
                for(std::size_t i=0;i<state->jobs.size();++i) {
                    WorkInputs inputs;bool ready=true,blocked=false;
                    {
                        std::lock_guard lock(state->mutex);auto& slot=state->slots[i];
                        if(slot.receipt.status!=WorkStatus::Queued)continue;
                        for(auto key:state->jobs[i].dependencies()) {
                            const auto& parent=state->slots[state->index.at(key)];
                            if(parent.receipt.status!=WorkStatus::Succeeded) {
                                ready=false;if(work_terminal(parent.receipt.status))blocked=true;
                            } else inputs.values_.emplace_back(key,*parent.value);
                        }
                        if(blocked){slot.receipt.status=WorkStatus::Blocked;slot.error=std::make_exception_ptr(WorkDependencyFailed{});}
                    }
                    if(blocked){state->completed.notify_all();state->resources.notify_waiters();continue;}
                    if(!ready || active>=state->limits.in_flight)continue;
                    const auto demand=state->jobs[i].demand();std::optional<CpuResourceCoordinator::Reservation> reservation;
                    try {reservation.emplace(state->resources.reserve(demand.workers,demand.buffer_bytes));}
                    catch(const CpuResourceBusy&){continue;}
                    {
                        std::lock_guard lock(state->mutex);auto& slot=state->slots[i];slot.receipt.status=WorkStatus::Running;
                        slot.receipt.queue_ns=ns(queued,Clock::now());
                        std::size_t current_jobs{},current_workers{};
                        for(std::size_t j=0;j<state->slots.size();++j)if(state->slots[j].receipt.status==WorkStatus::Running) {
                            ++current_jobs;current_workers=machine::checked_size_add(current_workers,state->jobs[j].peak_demand().workers);
                        }
                        // A declared admission bound for phased jobs; individual
                        // stage receipts describe their changing reservations.
                        current_workers=std::min(current_workers,state->resources.capacity());
                        state->peak_jobs=std::max(state->peak_jobs,current_jobs);state->peak_workers=std::max(state->peak_workers,current_workers);
                    }
                    try {
                        threads[i].emplace([state,i,inputs=std::move(inputs),reservation=std::move(*reservation)]() mutable {
                            executing_graph=state.get();const auto start=Clock::now();std::optional<WorkValue> value;std::exception_ptr error;bool cancelled=false;
                            const auto stop=state->slots[i].stop.get_token();std::unique_ptr<CpuResourceCoordinator> assigned;
                            try {
                                if(stop.stop_requested())throw ExecutionCancelled{};
                                if(state->jobs[i].definition_->invoke_phased)
                                    value=state->jobs[i].definition_->invoke_phased(inputs,state->resources,std::move(reservation),stop);
                                else {
                                    assigned.reset(new CpuResourceCoordinator(CpuResourceCoordinator::delegate(std::move(reservation))));
                                    value=state->jobs[i].definition_->invoke(inputs,*assigned,stop);
                                }
                                if(stop.stop_requested())throw ExecutionCancelled{};
                            } catch(const ExecutionCancelled&) {cancelled=true;error=std::current_exception();}
                            catch(...) {error=std::current_exception();}
                            { std::lock_guard lock(state->mutex);assigned.reset();auto& slot=state->slots[i];
                                slot.receipt.execution_ns=ns(start,Clock::now());slot.error=error;
                                slot.receipt.status=(cancelled || stop.stop_requested())?WorkStatus::Cancelled:(error?WorkStatus::Failed:WorkStatus::Succeeded);
                                if(slot.receipt.status==WorkStatus::Succeeded)slot.value=std::move(value);
                            }
                            state->completed.notify_all();state->resources.notify_waiters();executing_graph=nullptr;
                        });
                        ++active;
                    } catch(...) {
                        std::lock_guard lock(state->mutex);auto& slot=state->slots[i];slot.receipt.status=WorkStatus::Failed;slot.error=std::current_exception();
                        state->completed.notify_all();state->resources.notify_waiters();
                    }
                }
                state->completed.notify_all();state->resources.wait_for_change(epoch);
            }
        } catch(...) {
            const auto error=std::current_exception();state->cancel_all();
            for(auto& thread:threads)if(thread && thread->joinable())thread->join();
            std::lock_guard lock(state->mutex);
            for(auto& slot:state->slots)if(!work_terminal(slot.receipt.status)){slot.receipt.status=WorkStatus::Failed;slot.error=error;}
        }
        { std::lock_guard lock(state->mutex);state->done=true; }
        state->completed.notify_all();state->resources.notify_waiters();
    }
    std::vector<WorkJob> jobs_;WorkGraphLimits limits_;std::map<WorkKey,std::size_t> index_;
};
} // namespace rfc::runtime
