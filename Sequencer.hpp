/*
* This is a C++ version of the canonical pthread service to toggle a GPIO pin 
* every 100ms using different methods. It intends to abstract the service 
* management functionality and sequencing for ease of use. Much of the code is 
* left to be implemented by the student.
*
* Build with g++ --std=c++23 -Wall -Werror -pedantic
* Steve Rizor 3/16/2025
* Edited by : Sriya Garde & Vishnu Kumar
*/

 #pragma once

 #include <cstdint>
 #include <functional>
 #include <thread>
 #include <vector>
 #include <semaphore>
 #include <chrono>
 #include <limits>
 #include <iostream>
 #include <syslog.h>
 
 // The service class contains the service function and service parameters
 // (priority, affinity, etc). It spawns a thread to run the service, configures
 // the thread as required, and executes the service whenever it gets released.
 class Service
 {
    public:
        template<typename T>
        Service(T&& doService, uint8_t affinity, uint8_t priority, uint32_t period) 
        : _doService(std::forward<T>(doService)), 
            _affinity(affinity), 
            _priority(priority), 
            _period(period),
            _deadline_miss(0),
            _semaphore(0), 
            _running(true),
            _minExecTime(std::chrono::nanoseconds::max()),
            _maxExecTime(std::chrono::nanoseconds::zero()),
            _totalExecTime(std::chrono::nanoseconds::zero()),
            _executionCount(0),
            _lastStartTime(std::chrono::steady_clock::time_point::min()),
            _maxStartJitter(std::chrono::nanoseconds::zero()),
            _minStartJitter(std::chrono::nanoseconds::max())
        {        
            _service = std::jthread(&Service::_provideService, this);
            std::cout<<"New thread launched\n";
        }

        void stop();

        void release();

        int get_period() { return _period; } // Added getter for period
        void join();

    private:
        std::function<void(void)> _doService;
        uint8_t _affinity;
        uint8_t _priority;  
        uint32_t _period;
        uint32_t _deadline_miss;
        std::binary_semaphore _semaphore;
        std::atomic<bool> _running;
        std::jthread _service;

        std::chrono::nanoseconds _minExecTime;
        std::chrono::nanoseconds _maxExecTime;
        std::chrono::nanoseconds _totalExecTime;
        size_t _executionCount;

        std::chrono::steady_clock::time_point _lastStartTime;
        std::chrono::nanoseconds _maxStartJitter;
        std::chrono::nanoseconds _minStartJitter;

        void _initializeService();
        void _provideService();
        void _logStatistics();
 };
 
 // The sequencer class contains the services set and manages
 // starting/stopping the services. While the services are running,
 // the sequencer releases each service at the requisite timepoint.
 class Sequencer
 {
 public:
    template<typename... Args>
    void addService(Args &&... args)
    {
        _services.emplace_back(std::make_unique<Service>(std::forward<Args>(args)...));
    }

    void startServices();
    void stopServices();

    const std::vector<std::unique_ptr<Service>>& getServices() const {
        return _services;
    }
    
    int get_running() const {
        return _running;
    }
    
 private:
    std::vector<std::unique_ptr<Service>> _services;
    std::atomic<bool> _seq_running{false};
    std::jthread timer_thread;
    timer_t posix_timer;

    int _running = 1;  // Global variable to control service execution
    int exit_flag = 0;  // Global variable to control service execution
 
    static void timer_irq_service(union sigval sv);
    void timer_service();
 };
 