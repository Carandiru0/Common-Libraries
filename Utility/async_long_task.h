#pragma once
#include <Utility/class_helper.h>
#include <tbb/tbb.h>
#include <atomic>
#include <Random/superrandom.hpp>

#ifndef NDEBUG
#include "globals.h"

#define VERBOSE_LOGGING_ASYNC_LONG_TASK 0

#endif

#ifdef TRACY_ENABLE

#include <tracy.h>
#include <fmt/fmt.h>

#endif

enum thread_id_t {
	background_critical = 0,
	background = 1,

	thread_count
};

using task_id_t = int64_t;

namespace internal_only
{
	class async_work : no_copy
	{
		task_id_t /*const*/ _id;
	public:
		task_id_t const id() const { return(_id); }

		virtual void				execute() const = 0;

		async_work()
			: _id(PsuedoRandomNumber64() + int64_t((ptrdiff_t)(&(*this)))) // further the randomization probability problem with *this memory address and always != 0
		{
			[[unlikely]] if (0 == _id) // gayrantee id is never equal to zero
				++_id;
		}

		// move allowed only //
		async_work(async_work&& ref)
		: _id(std::move(ref._id))
		{
			ref._id = 0;
		}
		async_work& operator=(async_work&& ref) {
			_id = std::move(ref._id);
			ref._id = 0;

			return(*this);
		}
	};

	template <typename lambda>
	class async_work_lambda final : public async_work
	{
		lambda const _work;

	public:
		virtual void execute() const override final
		{
			_work();
		}

		async_work_lambda(lambda&& work_)
			: _work(std::move(work_))
		{}
	};
}

static constexpr uint32_t const HISTORY_SZ = 8;

class async_long_task : no_copy
{
public:
	enum beats
	{
		yield = 0,		// SleepEx(0, FALSE) returns immediately but reliquinshes the time slice for the thread.
		minimum = 1,	// 1ms
		frame = 16,		// 16ms 
		half_second = 500,
		full_second = 1000
	};
private:
	enum async_priority_t
	{
		priority_critical = THREAD_PRIORITY_TIME_CRITICAL,
		priority_normal = THREAD_PRIORITY_NORMAL,
		priority_idle = THREAD_PRIORITY_IDLE
	};
	
public:
	template <thread_id_t const thread, typename lambda> // ** use if don't care lambda/function is unique 
	static inline task_id_t const enqueue(lambda&& work_) {  // returns task_id
		static internal_only::async_work_lambda<lambda> instance(std::forward<lambda&&>(work_));
		return(_enqueue<thread>( &instance ));
	}
	template <thread_id_t const thread, uint32_t const unique_id, typename lambda> // ** use if lambda/function is required to be distinct from another invocation of the same lambda. unique_id is used to define each invocation...
	static inline task_id_t const enqueue(lambda&& work_) {  // returns task_id
		static internal_only::async_work_lambda<lambda> instance(std::forward<lambda&&>(work_));
		return(_enqueue<thread>(&instance));
	}
	template<thread_id_t const thread, bool const test_only = false>  // returns true when task is pending or active, false if finished or does not exist
	static inline bool const wait(task_id_t const task, [[maybe_unused]] std::string_view const context) {  // equal to ZERO equals wait for nothing, task ids

#if defined(NDEBUG)
		(void)context; // unused
#endif
		static constexpr uint32_t TRY_COUNT(2);

		[[unlikely]] if (0 == task) {
			return(false); // a task with an id of zero is invalid, return immediately
		}

		bool bFound(task == _current_task[thread]); // fast simple check

		if (!bFound) // not currently active task
		{
			// quick check if task has already completed, then no deep queue search required
			for (uint32_t i = 0; i < HISTORY_SZ; ++i) {
				if (task == _last_task[thread][i]) {
					return(false); // task already finished
				}
			}


			// deeper check into queues required
			tbb::concurrent_queue<internal_only::async_work const*>* __restrict queue_item_back(nullptr);

			if (!_items[thread].empty()) {

				queue_item_back = &_items[thread];
			}

			if (nullptr != queue_item_back) {

				tbb::concurrent_queue<internal_only::async_work const*> queue_item_forward;
				internal_only::async_work const* work(nullptr);

				{ // find task, if found give it highest priority in queue as first element
					uint32_t try_count(TRY_COUNT);
					while (0 != try_count && !queue_item_back->empty()) {

						if (queue_item_back->try_pop(work) && nullptr != work) {

							if (task == work->id()) {

								{ // make queue empty, deferring all other work
									internal_only::async_work const* deferred_work(nullptr);
									uint32_t nested_try_count(TRY_COUNT);
									while (0 != nested_try_count && !queue_item_back->empty()) {

										if (queue_item_back->try_pop(deferred_work)) {

											if (nullptr != deferred_work) {
												queue_item_forward.emplace(std::move(deferred_work));
											}
										}
										else {
											_mm_pause();
											--nested_try_count;
										}
									}
								}

								// move it back (first in line)
								queue_item_back->emplace(std::move(work));

								bFound = true;
								break;
							}
							else {
								// move it forward
								queue_item_forward.emplace(std::move(work));
							}
						}
						else {
							_mm_pause();
							--try_count;
						}
					}
				}

				{ // move it all back
					uint32_t try_count(TRY_COUNT);
					while (0 != try_count && !queue_item_forward.empty()) {

						if (queue_item_forward.try_pop(work)) {

							if (nullptr != work) {
								queue_item_back->emplace(std::move(work));
							}
						}
						else {
							_mm_pause();
							--try_count;
						}
					}
				}

				if (bFound) {

					if constexpr (test_only) {
						return(true); // task is pending
					}

					wake_up(thread);

					bool bAlerted(false);

					// wait until active
					while (task != _current_task[thread]) {

						if (queue_item_back->empty()) { // must be polled

							bFound = (task == _current_task[thread]);
							break;
						}

						// bugfix - must wakeup when idle for extended periods of time, and if program is not foreground task could have silently completed
						if (!bAlerted) {
							wake_up(thread);
							bAlerted = true;
						}
						_mm_pause();
					}
#if !defined(NDEBUG) && VERBOSE_LOGGING_ASYNC_LONG_TASK
					FMT_LOG_WARN(INFO_LOG, "{:s} {:s} task <{:d}> found [deep]",
						((background_critical == thread) ? "critical" : "background"), context, task);
#endif
				}
			}
		}
		
		
		if (bFound) { // found as active

			if constexpr (test_only) {
				return(true); // task is active
			}

			while (task == _current_task[thread]) {  // wait until current task clears
				
				// bugfix - must wakeup when idle for extended periods of time, and if program is not foreground task could have silently completed
				wake_up(thread);
				_mm_pause();
			}

		}
#if !defined(NDEBUG) && VERBOSE_LOGGING_ASYNC_LONG_TASK
		else {

			for (uint32_t i = 0; i < HISTORY_SZ; ++i) {

				if (task == _last_task[thread][i]) {
					bFound = true;  // leveraging same boolean variable doesn't imply anything
					break;
				}
			}

			
			if (!bFound) {

				if constexpr (test_only) {
					return(false); // task does not exist is already done or never existed in history[HISTORY_SZ]
				}
				else {
					FMT_LOG_FAIL(INFO_LOG, "{:s} {:s} task <{:d}> **not** found",
						((background_critical == thread) ? "critical" : "background"), context, task);
				}
			}
		}
#endif
		return(false); // task is finished
	}
	template<thread_id_t const thread>  // returns true when task is pending or active, false if finished or does not exist
	static inline bool const test(task_id_t const task, [[maybe_unused]] std::string_view const context) { // shortcut to test-only 
#if defined(NDEBUG)
		(void)context; // unused
#endif
		return(wait<thread, true>(task, context));
	}
	static bool const initialize(unsigned long const (&cores)[2], uint32_t const thread_stack_size = 0); // must be called once when appropriate at program start-up
	static void cancel_all();
	static bool const wait_for_all(milliseconds const timeout = ((milliseconds)UINT32_MAX) );  // optional timeout parameter - default is infinite, 0 would simply return if a wait is needed, all other alues are milliseconds
private:
	template<thread_id_t const thread>
	static inline task_id_t const _enqueue(internal_only::async_work const* const __restrict work_);

	static void wake_up(thread_id_t const thread);
private:
	template<thread_id_t const thread>
	static void background_task();

	template<thread_id_t const thread>
	static inline void process_async_queue(tbb::concurrent_queue< internal_only::async_work const* >& q);

	template<thread_id_t const thread>
	static inline void process_work();

	template<thread_id_t const thread>
	static void __cdecl background_thread(void*);
	
	template<thread_id_t const thread>
	static void record_task_id(task_id_t const task);
private:
	constinit static std::atomic_flag						_alive[thread_count];
	constinit static void*									_hQueued[thread_count];
	constinit static void*									_hThread[thread_count];
	constinit static tbb::atomic<int32_t>					_current_priority[thread_count];
	constinit static tbb::atomic<task_id_t>					_current_task[thread_count],
															_last_task[thread_count][HISTORY_SZ];

	static tbb::concurrent_queue< internal_only::async_work const* > _items[thread_count];
	
public:
	~async_long_task();
};

template <thread_id_t const thread>
inline task_id_t const async_long_task::_enqueue(internal_only::async_work const* const __restrict work_) {  // returns task_id

	task_id_t const task(work_->id()); // must grab id b4 move below
	_items[thread].emplace(std::move(work_));
	wake_up(thread);

	return(task);
}

#ifdef ASYNC_LONG_TASK_IMPLEMENTATION

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef _STL_WIN32_WINNT
#define _STL_WIN32_WINNT _WIN32_WINNT
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN	// Exclude rarely-used stuff from Windows headers
#endif

#define NOMINMAX

#include <Windows.h>

inline tbb::concurrent_queue< internal_only::async_work const* >		async_long_task::_items[thread_count];
constinit inline std::atomic_flag										async_long_task::_alive[thread_count]{};
constinit inline tbb::atomic<int32_t>									async_long_task::_current_priority[thread_count]{0};
constinit inline tbb::atomic<task_id_t>									async_long_task::_current_task[thread_count]{ 0 };
constinit inline tbb::atomic<task_id_t>									async_long_task::_last_task[thread_count][HISTORY_SZ]{ 0 };
constinit inline void*													async_long_task::_hThread[thread_count]{ nullptr };
constinit inline void*													async_long_task::_hQueued[thread_count]{ nullptr };

namespace // private to this file (anonymous)
{
	constinit static inline async_long_task	_async_long_task_owner{};		// the only private inaccesible instance
}

void async_long_task::wake_up(thread_id_t const thread)
{
	[[likely]] if (nullptr != _hThread[thread]) {
	
		// set priority
		if (background_critical == thread)
		{
			// resume critical
			if (priority_critical != _current_priority[thread]) {
				_current_priority[thread] = priority_critical;
				SetThreadPriority(_hThread[thread], priority_critical);
			}
		}
		else
		{
			// resume normal
			if (priority_normal != _current_priority[thread]) {
				_current_priority[thread] = priority_normal;
				SetThreadPriority(_hThread[thread], priority_normal);
			}
		}

		SetEvent(_hQueued[thread]);
	}
}

template<thread_id_t const thread>
inline void async_long_task::process_async_queue(tbb::concurrent_queue< internal_only::async_work const* >& q)
{
	internal_only::async_work const* work(nullptr);
	while (q.try_pop(work)) {

		[[likely]] if (nullptr != work) {
			
			//---------------------------------------------
			task_id_t const task(work->id());
			async_long_task::_current_task[thread] = task;

			//if constexpr (background_critical == thread) {

				work->execute();

			//}
			//else {
				//SetThreadPriority(_hThread[thread], THREAD_MODE_BACKGROUND_BEGIN); // this really slows down the background processing so it has been disabled. Only to be re-enabled if performance problems arise. (all commented out code here)
				
				// tbb should not be used in background tasks, or at least limited to 1 thread, being this background thread
				//work->execute();

				//SetThreadPriority(_hThread[thread], THREAD_MODE_BACKGROUND_END);
			//}
			
			record_task_id<thread>(task);
			async_long_task::_current_task[thread] = 0;

			//---------------------------------------------//

			// not dynamically allocated, just clear pointer.
			work = nullptr;
		}
	}
}

template<thread_id_t const thread>
inline void async_long_task::process_work()
{
	// background (critical or normal) tasks // 
	process_async_queue<thread>(_items[thread]);
}

template<thread_id_t const thread>
void async_long_task::background_task()
{
#ifdef TRACY_ENABLE
	ZoneScoped;
#endif
	process_work<thread>();

	// always revert to idle priority
	[[likely]] if (nullptr != _hThread[thread]) {

		_current_priority[thread] = priority_idle;
		SetThreadPriority(_hThread[thread], priority_idle);			// lowest idle cpu usage
	}
}

extern __declspec(noinline) void local_init_tbb_floating_point_env();  // external forward decl

template<thread_id_t const thread>
void __cdecl async_long_task::background_thread(void*)
{
	local_init_tbb_floating_point_env();

#ifdef TRACY_ENABLE
	__SetThreadName( fmt::format("async {:s} thread", ((0 == thread) ? "background critical" : "background")).c_str() );
#endif

	[[likely]] while (_alive[thread].test_and_set()) // if returns false, means flag was cleared - signaling its time to exit thread
	{
		// wait until work is queued
		WaitForSingleObject(_hQueued[thread], INFINITE); // *bugfix: better than an APC and SleepEx w/alertable state. Less latency, simpler.
		
		// do the work //
		background_task<thread>();
	}
	_alive[thread].clear(); // signal a clean exit
	_endthread();
}

template<thread_id_t const thread>
void async_long_task::record_task_id(task_id_t const task)
{
	constinit static uint32_t index[thread_count]{ 0 };	// unique to thread only by template parameter

	_last_task[thread][index[thread]] = task;

	index[thread] = (index[thread] + 1) & (HISTORY_SZ - 1);
}

bool const async_long_task::initialize(unsigned long const (&cores)[2], uint32_t const thread_stack_size)
{
	// initialized state must be done before creation of thread
	_alive[background_critical].test_and_set(); // set to alive state so thread doesn't immediately exit
	_alive[background].test_and_set(); // set to alive state so thread doesn't immediately exit

	_hQueued[background_critical] = CreateEvent(nullptr, FALSE, FALSE, nullptr); // queued signal per thread
	_hQueued[background] = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	_hThread[background_critical] = (void* const)_beginthread(&async_long_task::background_thread<background_critical>, thread_stack_size, nullptr);
	_hThread[background] = (void* const)_beginthread(&async_long_task::background_thread<background>, thread_stack_size, nullptr);
	
	if (_hThread[background_critical] && _hThread[background]) {

		if (0 != cores[0]) {
			SetThreadSelectedCpuSets(_hThread[background_critical], &cores[0], 1);
		}
		if (0 != cores[1]) {
			SetThreadSelectedCpuSets(_hThread[background], &cores[1], 1);
		}

		return(true);
	}

	return(false);
}

bool const async_long_task::wait_for_all(milliseconds const timeout)
{
	tTime const tStart(critical_now());

	bool bWaitState(false);

	while( (bWaitState = !(_items[background_critical].empty() & _items[background].empty())) )
	{
		_mm_pause(); // hint to processor
		SleepEx(beats::yield, FALSE); // lower cpu usage, yield to another thread

		[[unlikely]] if (critical_now() - tStart >= duration_cast<nanoseconds>(timeout)) {
			break;
		}
	}


	return(bWaitState);
}

void async_long_task::cancel_all()
{
	// abort any items in queue
	_items[background_critical].clear(); _items[background].clear();
	// dummy apc call which will exit the thread
	wake_up(background_critical); wake_up(background);

	SleepEx(beats::yield, FALSE); // yield to the threads
}

async_long_task::~async_long_task() 
{
#if !defined(NDEBUG) && VERBOSE_LOGGING_ASYNC_LONG_TASK
	FMT_LOG_WARN(INFO_LOG, "async long task threads  quitting...");
#endif

	// signal thread loop to exit //
	_alive[background_critical].clear(); _alive[background].clear();

	cancel_all();

	// wait until signalled for clean exit //
	for (uint32_t current_thread = 0; current_thread < thread_count; ++current_thread) {
		
		while (_alive[current_thread].test_and_set()) {

			_mm_pause(); // hint to processor
			SleepEx(beats::yield, FALSE); // lower cpu usage, yield to another thread
		}

		CloseHandle(_hQueued[current_thread]); _hQueued[current_thread] = nullptr;
	}
}


#endif // ASYNC_LONG_TASK_IMPLEMENTATION
