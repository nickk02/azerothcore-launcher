#pragma once
#include <coroutine>
#include <exception>
#include <utility>

namespace Core
{
    // A minimal, eagerly-started, single-awaiter coroutine task for async
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
    template <typename T>
    struct Task
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
            std::suspend_never initial_suspend() noexcept { return {}; }
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
        ~Task() { if (handle) handle.destroy(); }

        bool await_ready() const noexcept { return handle.done(); }
        void await_suspend(std::coroutine_handle<> awaiter) const noexcept { handle.promise().continuation = awaiter; }
        T await_resume()
        {
            if (handle.promise().error)
                std::rethrow_exception(handle.promise().error);
            return std::move(handle.promise().value);
        }
    };
}
