#include <sigslot/sigslot.h>

/**
 *
 * First, some boilerplate.
 *
 * This is just a trivial coroutine type, with immediate execution.
 *
 * It's more or less a copy of the sync<> type from:
 * https://kirit.com/How%20C%2B%2B%20coroutines%20work/My%20first%20coroutine
 */

template<typename T>
struct tasklet {
    struct promise_type;
    using handle_type = std::experimental::coroutine_handle<promise_type>;
    handle_type coro;

    tasklet(handle_type h) : coro(h) {
    }
    tasklet(tasklet && other) : coro(other.coro) {
        other.coro = nullptr;
    }
    tasklet &operator = (tasklet && other) {
        coro = other.coro;
        other.coro = nullptr;
    }
    tasklet(tasklet const &) = delete;
    tasklet & operator = (tasklet const &) = delete;

    ~tasklet() {
        if (coro) coro.destroy();
    }

    T get() {
        return coro.promise().value;
    }

    struct promise_type {
        T value;
        promise_type() : value() {
        }
        auto get_return_object() {
            return tasklet<T>{handle_type::from_promise(*this)};
        }
        auto return_value(T v) {
            value = v;
            return std::experimental::suspend_never{};
        }
        auto final_suspend() {
            return std::experimental::suspend_always{};
        }
        auto initial_suspend() {
            return std::experimental::suspend_never{};
        }
        void unhandled_exception() {
            std::exit(1);
        }
    };
};

#include <iostream>

/**
 * We'll use some global signals here.
 */
sigslot::signal<> tick;
sigslot::signal<int> tock;
sigslot::signal<int, std::string> splat;

/**
 * Our simple coroutine.
 *
 * All it's going to do is await the two signals - we won't do anything with it.
 */

tasklet<int> coroutine_example() {
    std::cout << "C: Ready." << std::endl;
    /**
     * If you co_await a signal, execution stops and control moves to the caller.
     */
    co_await tick;
    /**
     * And now the signal must have been triggered.
     *
     * Awaiting a signal is inherently one-shot, if the signal is triggered twice,
     * we won't know about it.
     */
    std::cout << "C: Got a tick." << std::endl;
    /**
     * For signals with a single argument, the argument gets returned by the
     * co_await when it completes:
     */
    int foo = co_await tock;
    std::cout << "C: Got a tock of " << foo << std::endl;
    /**
     * Signals that have multiple arguments also work, but it passes back a std::tuple
     * in this case. This is relatively easy to unwrap with a std::tie, though it would
     * be simpler with C++17's Structured Bindings.
     */
    int x;
    std::string s;
    std::tie(x, s) = co_await splat;
    std::cout << "C: Got a splat of " << x << ", " << s << std::endl;
    co_return foo;
}

int main(int argc, char *argv[]) {
    try {
        /**
         * First with the coroutine awaiting:
         */
        std::cout << "M: Executing coroutine." << std::endl;
        auto c = coroutine_example(); // Start the coroutine. It'll execute until it needs to await a signal, then stop and return.
        std:: cout << "M: Tick:" << std::endl;
        tick(); // When we emit the signal, it'll start executing the coroutine again. Again, it'll stop when it awaits the next signal.
        std::cout << "M: Tock(42):" << std::endl;
        tock(42);
        std::cout << "M: Splat(17, \"Gerbils\")" << std::endl;
        splat(17, "Gerbils");
        std::cout << "M: Answer is " << c.get() << std::endl;
        /**
         * If we sent the second signal before the first, the coroutine would wait forever.
         * This is because it wouldn't fire when the coroutine is suspended in co_await.
         */
    } catch (std::exception const & e) {
        std::cerr << e.what() << std::endl;
        throw;
    }
}