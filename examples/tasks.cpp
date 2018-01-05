#include <iostream>
#include <chrono>
#include <thread>

#include "canary.hpp"

struct Task {
    std::string emoji;
    std::string msg;
    std::function<void()> fn;

    Task(std::string emoji, std::string msg, std::function<void()> fn) : emoji(emoji), msg(msg), fn(fn) {}
    Task(std::string msg, std::function<void()> fn) : emoji(""), msg(msg), fn(fn) {}

    void operator()() {
        fn();
    }
};

// Execute the first task and then start recursion on the next one
template<class F, class... Fs>
void DoTask(size_t size, size_t pos, F fn, Fs... fns) {
    DoTask(size, pos, fn);

    if (size != pos) {
        DoTask(size, pos+1, fns...);
    }
}

// Print the messages for this one task and then execute it
template<class F>
void DoTask(size_t size, size_t pos, F fn) {
    // Task
    {
        Canary::Ansi::Faint faint(std::cout);
        std::cout << "[" << pos << "/" << size << "] ";
    }

    // Message
    if (!fn.emoji.empty()) {
        std::cout << fn.emoji << " ";
    }
    std::cout << fn.msg << std::endl;

    // Execute
    fn();
}

// Execute and pretty print a list of tasks
template<class... Fs>
void ExecuteTasks(Fs... fns) {
    // Start time measurement
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    // Start execution
    DoTask(sizeof...(fns), 1, fns...);

    // Stop measurement
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    // Green finished
    {
        Canary::Ansi::GreenForeground red(std::cout);
        std::cout << "Finished." << std::endl;
    }

    // Print time
    std::cout << Canary::Emoji::zap
              << " Done in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms."
              << std::endl;
}

int main(int argc, char** argv) {
    // Execute some fake tasks
    ExecuteTasks(
        Task(Canary::Emoji::truck, "Task 1", []() {
            // Do fake calculations
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }),

        Task(Canary::Emoji::package, "Task 2", []() {
            // Do fake calculations
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }),

        Task(Canary::Emoji::alien, "Task 3", []() {
            // Do fake calculations
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }),

        Task(Canary::Emoji::sparkles, "Task 4", []() {
            // Do fake calculations
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        })
    );
}
