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

#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
#include <sched.h>

#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
 
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "Sequencer.hpp"
#include <atomic>
#include <syslog.h>

#include <fstream>
#include <linux/gpio.h>
 
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
 
#define DEV_NAME "/dev/gpiochip0"

// int _running = 1;  // Global variable to control service execution

// Service method definitions:
void Service::_initializeService(){

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(_affinity, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Error setting thread affinity" << std::endl;
        syslog(LOG_ERR, "Error setting thread affinity\n");
    }

    // Set thread priority and scheduling policy
    struct sched_param sched;
    sched.sched_priority = _priority;
    
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched) != 0) {
        std::cerr << "Error setting thread priority" << std::endl;
        syslog(LOG_ERR, "Error setting thread priority\n");
    }

    std::cout << "Service initialized with affinity: " << (int)_affinity 
            << ", priority: " << (int)_priority 
            << ", period: " << _period << "ms\n";
    syslog(LOG_CRIT, "Service initialized with Affinity: %d, Priority: %d, Period: %dms, ThreadID: %lu\n",
            (int)_affinity, 
            (int)_priority, 
            _period,
            syscall(SYS_gettid));
}

void Service::join() {
    if (_service.joinable())
        _service.join();
}

void Service::stop(){
    _running = false;  // Mark the service as stopped
    _logStatistics();
}

void Service::release(){
    _semaphore.release();  // Allow service to run
}

void Service::_provideService() {
    _initializeService();

    while (_running) {
        if (_semaphore.try_acquire_for(std::chrono::milliseconds(100))) {
            
            // Run the actual service task
            auto startTime = std::chrono::steady_clock::now();
            
            syslog(LOG_CRIT, "Task launch thread id: %lu", syscall(SYS_gettid));
            
            setRunning(true);  // Set the service as running
            
            _doService();
            
            setRunning(false);  // Set the service as running

            
            auto endTime = std::chrono::steady_clock::now();

            auto execTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
            syslog(LOG_CRIT, "Task completed thread id: %lu -> execution time: %ld μS", syscall(SYS_gettid), execTime.count()/1000);
            
            _lastStartTime = startTime;
            
            
            // Update execution time statistics
            if (execTime < _minExecTime)
                _minExecTime = execTime;
            if (execTime > _maxExecTime)
                _maxExecTime = execTime;

            _totalExecTime += execTime;
            _executionCount++;
        }
    }
}

void Service::_logStatistics() {
    using namespace std::chrono;

    nanoseconds expected = std::chrono::milliseconds(_period);
    auto avg = (_executionCount > 0) ? nanoseconds(_totalExecTime.count() / _executionCount) : nanoseconds(0);

    // Calculate jitter
    auto Jitter = _maxExecTime - _minExecTime;

    syslog(LOG_CRIT,
        "Service stats -> Tid: %ld, affinity: %d, priority: %d, period: %d\n"
        "Executions: %lu\n"
        "Exec Time Min: %ld μs, Max: %ld μs, Avg: %ld μs\n"
        "Jitter : %ld μs, \n"
        "Deadline Misses: %d\n",
        syscall(SYS_gettid),
        _affinity,
        _priority,  
        _period,
        _executionCount,
        duration_cast<microseconds>(_minExecTime).count(),
        duration_cast<microseconds>(_maxExecTime).count(),
        duration_cast<microseconds>(avg).count(),
        duration_cast<microseconds>(Jitter).count(),
        _deadline_miss_count
    );
}

// Sequencer method definitions:
// Timer service that will launch based on set interval timer
void Sequencer::timer_irq_service(union sigval sv){
    Sequencer* sequencer = static_cast<Sequencer*>(sv.sival_ptr);  // Retrieve the sequencer instance
    static int count = 0;

    if(!sequencer->get_running()) {
        exit(0);  // Exit if the exit flag is set
    }

    count+= 1;  // Increment the count by 10 ms

    for (auto& service : sequencer->getServices()){
        // Check if the service is running and if the period has elapsed
        int val = count % service->get_period();

        if (val == 0) {
            if(service->isRunning()){
                syslog(LOG_ERR, "Deadline miss detected! Service with period %d ms is still running.", service->get_period());
                service->add_deadline_miss();
            } else {
                service->release();  // Start the service if not already running
            }
        }
    }
}

void Sequencer::startServices(){
    _running = 1;  // Set the running flag to true

    // Set up the timer to call the timer_irq_service function
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_THREAD;//run this in a seperate thread
    sev.sigev_value.sival_ptr = this; //store pointer of current object instance to pass to handler
    sev.sigev_notify_function = timer_irq_service;

    if (timer_create(CLOCK_MONOTONIC, &sev, &posix_timer) == -1)
    {
        perror("timer_create");
        return;
    }

    // Configure the timer to expire after 10 msec and then every 10 msec
    struct itimerspec its{};
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1000000; 
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 1000000;

    if (timer_settime(posix_timer, 0, &its, nullptr) == -1)
    {
        perror("timer_settime");
        return;
    }
}

void Sequencer::stopServices(){
    _running = 0;  // Set the running flag to false

    for (auto& service : _services) {
        service->stop();  // Stop the service
    }
    
    std::cout<<"Services stopped, Joining\n";
    syslog(LOG_CRIT, "Services stopped, Joining\n");
    
    // for (auto& service : _services) {
    //     service->join();  // Stop the service
    // }
}

