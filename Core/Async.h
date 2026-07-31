#pragma once
#include <cassert>
#include <coroutine>
#include <exception>
#include <utility>

namespace Core
{
    // A minimal, lazily-started, single-awaiter coroutine task for async
    // Core:: methods that need to return a plain C++ value (not a
    // WinRT-projected type).
    //
    // winrt::Windows::Foundation::IAsyncOperation<T> cannot be used for this:
    // it requires T to satisfy cppwinrt's impl::has_category_v<T> so results
    // can be marshaled across a COM ABI vtable (confirmed via a real build:
    // "TResult must be WinRT type" -- the same class of constraint already
    // documented in RealmStatusChecker.h for plain enums). std::vector<T> has
    // no cppwinrt category specialization for ANY T -- confirmed by grepping
    // the Windows SDK's cppwinrt headers, zero hits for
    // "struct category<std::vector" anywhere -- so
    // IAsyncOperation<std::vector<T>> can never compile, regardless of
    // whether T itself is WinRT-compatible. This isn't specific to
    // RemoteAddon; it would fail even for IAsyncOperation<std::vector<int>>.
    //
    // Task<T> sidesteps the ABI requirement entirely: it's a plain C++20
    // coroutine type with its own promise_type, still fully co_await-able
    // from any other coroutine (WinRT-flavored or not), since co_await only
    // needs the awaited expression to be Awaitable -- independent of the
    // awaiting coroutine's own return type. Internals of a Task<T>-returning
    // function can still freely co_await real WinRT awaitables
    // (winrt::resume_background(), HttpClient::GetStringAsync(), etc.);
    // Task<T> only replaces the outermost return type, not what can be
    // awaited inside the function body. Verified correct (including a real
    // cross-thread resumption, nested Task<T> awaiting another Task<T>, and
    // exception propagation) against a standalone harness before adopting
    // this here.
    //
    // IMPORTANT -- no apartment marshaling: unlike IAsyncOperation<T>, which
    // captures the calling apartment and automatically resumes there,
    // Task<T> resumes its awaiter on whatever thread the coroutine happens
    // to finish on (e.g. a background thread pool thread if the body used
    // co_await winrt::resume_background() and never hopped back). Any caller
    // that touches UI/XAML state after `co_await`-ing a Task<T>-returning
    // method MUST wrap that touch in DispatcherQueue().TryEnqueue(...),
    // exactly like the existing pattern in Pages/HomePage.cpp's
    // CheckRealmStatusAsync. Task<T> deliberately does not add automatic
    // marshaling itself -- that would duplicate an established codebase
    // convention for no benefit.
    //
    // Lazy start (initial_suspend returns suspend_always) is load-bearing,
    // not a style choice: the coroutine body -- including any
    // co_await resume_background() that hops it onto another thread -- must
    // not begin running until this Task's own awaiter has already stored the
    // caller's continuation handle into the promise. If the body could start
    // eagerly (suspend_never) and race a background thread to final_suspend
    // before the continuation was stored, FinalAwaiter would resume
    // std::noop_coroutine() instead of the real caller and the caller would
    // hang forever. See await_suspend below for how the store-then-resume
    // ordering is enforced.
    template <typename T>
    struct [[nodiscard]] Task
    {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct FinalAwaiter
        {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept
            {
                auto continuation = h.promise().continuation;
                return continuation ? continuation : std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        struct promise_type
        {
            T value{};
            std::exception_ptr error;
            std::coroutine_handle<> continuation;

            Task get_return_object() { return Task{ handle_type::from_promise(*this) }; }
            // suspend_always: the coroutine body must not run until Task's
            // own await_suspend() has stored the caller's continuation (see
            // the class-level comment above) -- this is what makes the
            // lost-wakeup race structurally impossible rather than merely
            // unlikely.
            std::suspend_always initial_suspend() noexcept { return {}; }
            FinalAwaiter final_suspend() noexcept { return {}; }
            void return_value(T v) { value = std::move(v); }
            void unhandled_exception() { error = std::current_exception(); }
        };

        handle_type handle;

        explicit Task(handle_type h) noexcept : handle(h) {}
        Task(Task const&) = delete;
        Task& operator=(Task const&) = delete;
        Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (handle) handle.destroy();
                handle = std::exchange(other.handle, {});
            }
            return *this;
        }
        ~Task()
        {
            if (handle)
            {
                // Every current call site immediately co_awaits the Task<T>
                // it creates, so this coroutine is always done() by the time
                // it's destroyed. This assert exists for whoever writes the
                // next Task<T>-returning code and discards one mid-flight
                // instead: destroying a coroutine handle while it's still
                // suspended mid-body (e.g. parked on a background-thread hop)
                // is undefined behavior, not just a leak. Debug-only by
                // design -- this is a misuse guard, not real cancellation
                // support, which this type does not implement.
                assert(handle.done() && "Task<T> destroyed before it completed");
                handle.destroy();
            }
        }

        bool await_ready() const noexcept { return handle.done(); }
        // Store the caller's continuation BEFORE starting the coroutine body
        // (handle.resume()). This ordering is what closes the lost-wakeup
        // race: combined with initial_suspend() returning suspend_always
        // above, the coroutine body provably cannot reach final_suspend --
        // on this thread or any other it hops to -- until after
        // `continuation` is already stored. No atomics/locking needed; the
        // race window is removed by construction, not synchronized around.
        void await_suspend(std::coroutine_handle<> awaiter) const noexcept
        {
            handle.promise().continuation = awaiter;
            handle.resume();
        }
        T await_resume()
        {
            if (handle.promise().error)
                std::rethrow_exception(handle.promise().error);
            return std::move(handle.promise().value);
        }
    };
}
