#ifndef APEX_POLICIES_H  
#define APEX_POLICIES_H  

#include "apex_api.hpp"
#include "apex_export.h"
#include "utils.hpp"
#include <stdint.h>
#include <fstream>
#include <memory>
#include <boost/atomic.hpp>
#include <list>
#include <map>

#ifdef APEX_HAVE_ACTIVEHARMONY
#include "hclient.h"
#endif


extern bool apex_throttleOn;         // Current Throttle status
extern bool apex_checkThrottling;    // Is thread throttling desired
extern bool apex_energyThrottling;   // Try to save power while throttling

typedef enum {INITIAL_STATE, BASELINE, INCREASE, DECREASE, NO_CHANGE} last_action_t;

typedef uint32_t apex_tuning_session_handle;

#define APEX_HIGH_POWER_LIMIT  220.0  // system specific cutoff to identify busy systems, WATTS
#define APEX_LOW_POWER_LIMIT   200.0  // system specific cutoff to identify busy systems, WATTS

#define APEX_MAX_THREADS 24
#define APEX_MIN_THREADS 1
#define MAX_WINDOW_SIZE 3

enum class apex_param_type : int {NONE, LONG, DOUBLE, ENUM};
enum class apex_ah_tuning_strategy : int {EXHAUSTIVE, RANDOM, NELDER_MEAD, PARALLEL_RANK_ORDER};

struct apex_tuning_session;
class apex_tuning_request;

class apex_param {
    protected:
        const std::string name;

    public:
        apex_param(const std::string & name) : name{name} {};
        virtual ~apex_param() {};

        const std::string & get_name() const {
            return name;
        };

        virtual const apex_param_type get_type() const {
            return apex_param_type::NONE;
        };
        
        friend apex_tuning_session_handle __setup_custom_tuning(apex_tuning_request & request);
        friend int __common_setup_custom_tuning(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
        friend int __active_harmony_custom_setup(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
};

class apex_param_long : public apex_param {
    protected:
        std::shared_ptr<long> value;
        const long min;
        const long max;
        const long step;

    public:
        apex_param_long(const std::string & name, const long init_value,
                        const long min, const long max, const long step)
            : apex_param(name), value{std::make_shared<long>(init_value)},
              min{min}, max{max}, step{step} {};
        virtual ~apex_param_long() {};

        const long get_value() const {
            return *value;    
        };

        virtual const apex_param_type get_type() const {
            return apex_param_type::LONG;
        };
        friend int __active_harmony_custom_setup(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
};

class apex_param_double : public apex_param {
    protected:
        std::shared_ptr<double> value;
        const double min;
        const double max;
        const double step;

    public:
        apex_param_double(const std::string & name, const double init_value,
                        const double min, const double max, const double step)
            : apex_param(name), value{std::make_shared<double>(init_value)},
              min{min}, max{max}, step{step} {};
        virtual ~apex_param_double() {};

        const double get_value() const {
            return *value;    
        };

        virtual const apex_param_type get_type() const {
            return apex_param_type::DOUBLE;
        };
        friend int __active_harmony_custom_setup(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
};

class apex_param_enum : public apex_param {
    protected:
        std::string init_value;
        std::shared_ptr<const char*> value;
        std::list<std::string> possible_values;

    public:
        apex_param_enum(const std::string & name, const std::string & init_value, const std::list<std::string> possible_values) :
              apex_param(name), init_value{init_value}, value{std::make_shared<const char*>(init_value.c_str())}, possible_values{possible_values} {};
        virtual ~apex_param_enum() {};

        const std::string get_value() const {
            return std::string{*value};
        };

        virtual const apex_param_type get_type() const {
            return apex_param_type::ENUM;
        };
        friend int __active_harmony_custom_setup(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
};


class apex_tuning_request {
    protected:
        std::string name;
        std::function<double()> metric;
        std::map<std::string, std::shared_ptr<apex_param>> params;        
        apex_event_type trigger;
        apex_tuning_session_handle tuning_session_handle;
        bool running;
        apex_ah_tuning_strategy strategy;
        

    public:
        apex_tuning_request(const std::string & name, std::function<double()> metric, apex_event_type trigger) 
            : name{name}, metric{metric}, trigger{trigger}, tuning_session_handle{0},
            running{false}, strategy{apex_ah_tuning_strategy::PARALLEL_RANK_ORDER}  {};
        apex_tuning_request(const std::string & name) : name{name}, trigger{APEX_INVALID_EVENT},
            tuning_session_handle{0}, running{false}, strategy{apex_ah_tuning_strategy::PARALLEL_RANK_ORDER} {};
        virtual ~apex_tuning_request()  {};

        std::shared_ptr<apex_param_long> add_param_long(const std::string & name, const long init_value, const long min,
                            const long max, const long step) {
            std::shared_ptr<apex_param_long> param{std::make_shared<apex_param_long>(name, init_value, min, max, step)};
            params.insert(std::make_pair(name, param));
            return param;
        };

        std::shared_ptr<apex_param_double> add_param_double(const std::string & name, const double init_value, const double min,
                              const double max, const double step) {
            std::shared_ptr<apex_param_double> param{std::make_shared<apex_param_double>(name, init_value, min, max, step)};
            params.insert(std::make_pair(name, param));
            return param;
        };

        std::shared_ptr<apex_param_enum> add_param_enum(const std::string & name, const std::string & init_value, const std::list<std::string> & possible_values) {
            std::shared_ptr<apex_param_enum> param{std::make_shared<apex_param_enum>(name, init_value, possible_values)};
            params.insert(std::make_pair(name, param));
            return param;
        };

        std::shared_ptr<apex_param> get_param(const std::string & name) const {
            auto search = params.find(name);
            if(search == params.end()) {
                return std::shared_ptr<apex_param>{};
            } else {
                return search->second;
            }
        };

        void set_metric(std::function<double()> m) {
            metric = m;
        };

        void set_trigger(apex_event_type t) {
            trigger = t;
        };

        std::function<double()> get_metric() const {
            return metric;
        };

        apex_event_type get_trigger() const {
            return trigger;
        };

        bool is_running() const {
            return running;
        };

        apex_tuning_session_handle get_session_handle() const {
            return tuning_session_handle;
        };

        apex_ah_tuning_strategy get_strategy() const {
            return strategy;
        };

        void set_strategy(apex_ah_tuning_strategy s) {
            strategy = s;
        };

        friend apex_tuning_session_handle __setup_custom_tuning(apex_tuning_request & request);
        friend int __common_setup_custom_tuning(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);
        friend int __active_harmony_custom_setup(std::shared_ptr<apex_tuning_session> tuning_session, apex_tuning_request & request);

};


struct apex_tuning_session {
    apex_tuning_session_handle id;

#ifdef APEX_HAVE_ACTIVEHARMONY
    hdesc_t * hdesc;
#else
    void * hdesc;
#endif

    int test_pp = 0;
    boost::atomic<bool> apex_energy_init{false};
    boost::atomic<bool> apex_timer_init{false};

    bool converged_message = false;

    // variables related to power throttling
    double max_watts = APEX_HIGH_POWER_LIMIT;
    double min_watts = APEX_LOW_POWER_LIMIT;
    int max_threads = APEX_MAX_THREADS;
    int min_threads = APEX_MIN_THREADS;
    int thread_step = 1;
    long int thread_cap = ::apex::hardware_concurrency();
    double moving_average = 0.0;
    int window_size = MAX_WINDOW_SIZE;
    int delay = 0;
    int throughput_delay = MAX_WINDOW_SIZE;

    // variables related to throughput or custom throttling
    apex_function_address function_of_interest = APEX_NULL_FUNCTION_ADDRESS;
    std::string function_name_of_interest = "";
    std::function<double()> metric_of_interest;
    apex_profile function_baseline;
    apex_profile function_history;
    last_action_t last_action = INITIAL_STATE;
    apex_optimization_criteria_t throttling_criteria = APEX_MAXIMIZE_THROUGHPUT;
    std::vector<std::pair<std::string,long*>> tunable_params;

    // variables for hill climbing
    double * evaluations = NULL;
    int * observations = NULL;
    std::ofstream cap_data;
    bool cap_data_open = false;

    // variables for active harmony general tuning
    long int *__ah_inputs[10]; // more than 10 would be pointless
    int __num_ah_inputs;
    apex_ah_tuning_strategy strategy = apex_ah_tuning_strategy::PARALLEL_RANK_ORDER;

    apex_tuning_session(apex_tuning_session_handle h) : id{h} {};
};


#endif // APEX_POLICIES_H  


