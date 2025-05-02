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

// std::jthread _service;  // Global thread for service execution

// Sequencer sequencer{};
// int exit_flag = 0;

// Fib sequence definitions

#define FIB_LIMIT_FOR_32_BIT 47
#define NSEC_PER_SEC (1000000000)
#define NSEC_PER_MSEC (1000000)
#define NSEC_PER_MICROSEC (1000)
#define FIB10_ITERATIONS 1500000
#define FIB20_ITERATIONS 3000000

unsigned int seqIterations = FIB_LIMIT_FOR_32_BIT;
unsigned int idx = 0, jdx = 1;
unsigned int fib = 0, fib0 = 0, fib1 = 1;
// unsigned int fib10Cnt = 0, fib20Cnt = 0;

// Service method definitions:
void Service::_initializeService(){

    // todo: set affinity, priority, sched policy
    // (heads up: the thread is already running and we're in its context right now)
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
    // todo: change state to "not running" using an atomic variable
    // (heads up: what if the service is waiting on the semaphore when this happens?)
    _running = false;  // Mark the service as stopped
}

void Service::release(){

    // todo: release the service using the semaphore
    _semaphore.release();  // Allow service to run
}

void Service::_provideService() {
    _initializeService();

    while (_running) {
        if (_semaphore.try_acquire_for(std::chrono::milliseconds(100))) {
            syslog(LOG_CRIT, "task launch thread id: %lu", syscall(SYS_gettid));
            auto startTime = std::chrono::steady_clock::now();
            
            _lastStartTime = startTime;
            
            // Run the actual service task
            _doService();
            syslog(LOG_CRIT, "task completed thread id: %lu", syscall(SYS_gettid));

            auto endTime = std::chrono::steady_clock::now();
            auto execTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

            // Update execution time statistics
            if (execTime < _minExecTime) 
                _minExecTime = execTime;
            if (execTime > _maxExecTime) 
                _maxExecTime = execTime;

            _totalExecTime += execTime;
            _executionCount++;
        }
    }

    _logStatistics();
}

void Service::_logStatistics() {
    using namespace std::chrono;

    nanoseconds expected = std::chrono::milliseconds(_period);
    auto avg = (_executionCount > 0) ? nanoseconds(_totalExecTime.count() / _executionCount) : nanoseconds(0);

    // Calculate jitter
    auto Jitter = _maxExecTime - _minExecTime;
    // auto minJitter = _minExecTime - expected;

    syslog(LOG_CRIT,
        "Service stats -> Tid: %ld\n"
        "Executions: %lu\n"
        "Exec Time Min: %ld μs, Max: %ld μs, Avg: %ld μs\n"
        "Jitter : %ld μs, \n",
        syscall(SYS_gettid),
        _executionCount,
        duration_cast<microseconds>(_minExecTime).count(),
        duration_cast<microseconds>(_maxExecTime).count(),
        duration_cast<microseconds>(avg).count(),
        duration_cast<microseconds>(Jitter).count()
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
            service->release();  // Trigger service execution immediately (you can add periodic logic here)
        }
    }
}

void Sequencer::startServices(){
    _running = 1;  // Set the running flag to true
    // todo: start timer(s), release services

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
    // todo: stop timer(s), stop services
    for (auto& service : _services) {
        service->stop();  // Stop the service
    }

    for (auto& service : _services) {
        service->join();  // Stop the service
    }

    std::cout<<"Services stopped\n";
    syslog(LOG_CRIT, "Services stopped\n");
}

