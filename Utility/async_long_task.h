#pragma once
#include <Utility/class_helper.h>
#include <tbb/concurrent_queue.h>
#include <tbb/task.h>
#include <tbb/atomic.h>
#include <tbb/scalable_allocator.h>
#include <atomic>
#include <Random/superrandom.hpp>

#ifndef NDEBUG
#include "globals.h"

#define VERBOSE_LOGGING_ASYNC_LONG_TASK 0

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

class alignas(32) async_long_task : no_copy
{
public:
	enum beats
	{
		min = 1,
		frame = 33,
		half = 500,
		full = 1000,
		max = full
	};

private:
	enum async_priority_t
	{
		priority_critical = THREAD_PRIORITY_TIME_CRITICAL,
		priority_normal = THREAD_PRIORITY_NORMAL,
		priority_idle = THREAD_PRIORITY_IDLE
	};
	
public:
	template <thread_id_t const thread, typename lambda>
	static inline task_id_t const enqueue(lambda&& work_) {  // returns task_id
		return(_enqueue<thread>(new internal_only::async_work_lambda(std::forward<lambda&&>(work_))));
	}
	template<thread_id_t const thread, bool const test_only = false>  // returns true when task is pending or active, false if finished or does not exist
	static inline bool const wait(task_id_t const task, [[maybe_unused]] std::string_view const context) {  // equal to ZERO equals wait for nothing, task ids

#if defined(NDEBUG)
		(void)context; // unused
#endif
		static constexpr uint32_t TRY_COUNT(2);

		[[unlikely]] if (0 == task)
			return(false); // doesn't exist

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

			bool bAlerted(false);

			while (task == _current_task[thread]) {  // wait until current task clears
				
				// bugfix - must wakeup when idle for extended periods of time, and if program is not foreground task could have silently completed
				if (!bAlerted) {
					wake_up(thread);
					bAlerted = true;
				}
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
	static bool const initialize(unsigned long const (&cores)[2]); // must be called once when appropriate at program start-up
	static void cancel_all();
	static bool const wait_for_all(milliseconds const timeout = ((milliseconds)UINT32_MAX) );  // optional timeout parameter - default is infinite, 0 would simply return if a wait is needed, all other alues are milliseconds
private:
	template<thread_id_t const thread>
	static inline task_id_t const _enqueue(internal_only::async_work const* const __restrict work_);

	static void wake_up(thread_id_t const thread);
private:
	static void __stdcall background_apc(unsigned long long);

	template<thread_id_t const thread>
	static inline void process_async_queue(tbb::concurrent_queue< internal_only::async_work const* >& q);

	template<thread_id_t const thread>
	static inline void process_work();

	template<thread_id_t const thread>
	static void __cdecl background_thread(void*);
	
	template<thread_id_t const thread>
	static void record_task_id(task_id_t const task);
private:
	static std::atomic_flag									_alive[thread_count];
	static void*											_hThread[thread_count];
	static tbb::atomic<int32_t>								_current_priority[thread_count];
	static tbb::atomic<task_id_t>							_current_task[thread_count],
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
#include <Windows.h>

inline tbb::concurrent_queue< internal_only::async_work const* >		async_long_task::_items[thread_count];
inline std::atomic_flag													async_long_task::_alive[thread_count]{};
inline tbb::atomic<int32_t>												async_long_task::_current_priority[thread_count]{0};
inline tbb::atomic<task_id_t>											async_long_task::_current_task[thread_count]{ 0 };
inline tbb::atomic<task_id_t>											async_long_task::_last_task[thread_count][HISTORY_SZ]{ 0 };
inline void*															async_long_task::_hThread[thread_count]{ nullptr };

namespace internal_only
{
	static inline async_long_task	_async_long_task_owner;		// the only private inaccesible instance
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

		if (0 == QueueUserAPC(&async_long_task::background_apc, _hThread[thread], thread)) // always posts to assigned thread handle
		{
#if !defined(NDEBUG) && VERBOSE_LOGGING_ASYNC_LONG_TASK
			DWORD const dwErr = GetLastError();
			FMT_LOG_WARN(INFO_LOG, "could not wake up {:s}, last error {:d}", 
				((background_critical == thread) ? "critical" : "background"),
				dwErr
				);
#endif
		}
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

			if constexpr (background_critical == thread) {

				work->execute();

			}
			else {
				SetThreadPriority(_hThread[thread], THREAD_MODE_BACKGROUND_BEGIN);
				
				// tbb should not be used in background tasks, or at least limited to 1 thread, being this background thread
				work->execute();

				SetThreadPriority(_hThread[thread], THREAD_MODE_BACKGROUND_END);
			}
			
			record_task_id<thread>(task);
			async_long_task::_current_task[thread] = 0;

			//---------------------------------------------//

			delete work;
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

void __stdcall async_long_task::background_apc(unsigned long long mode)
{
	thread_id_t const thread((thread_id_t const)mode);

	if (background_critical == thread) {
		process_work<background_critical>();
	}
	else {
		process_work<background>();
	}

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

	[[likely]] while (_alive[thread].test_and_set()) // if returns false, means flag was cleared - signaling its time to exit thread
	{
		SleepEx(INFINITE, TRUE); // always sleeping but in "alertable state", thread only runs when there is work (wake_up)

		// the software interrupt has been queued and is executing transparently / hidden //
	}
	_alive[thread].clear(); // signal a clean exit
}

template<thread_id_t const thread>
void async_long_task::record_task_id(task_id_t const task)
{
	static uint32_t index[thread_count]{ 0 };	// unique to thread only by template parameter

	_last_task[thread][index[thread]] = task;

	index[thread] = (index[thread] + 1) & (HISTORY_SZ - 1);
}

bool const async_long_task::initialize(unsigned long const (&cores)[2])
{
	// initialized state must be done before creation of thread
	_alive[background_critical].test_and_set(); // set to alive state so thread doesn't immediately exit
	_alive[background].test_and_set(); // set to alive state so thread doesn't immediately exit

	_hThread[background_critical] = (void* const)_beginthread(&async_long_task::background_thread<background_critical>, 0, nullptr);
	_hThread[background] = (void* const)_beginthread(&async_long_task::background_thread<background>, 0, nullptr);

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
	tTime tStart = critical_now();

	bool bWaitState(false);

	while( (bWaitState = !(_items[background_critical].empty() & _items[background].empty())) )
	{
		_mm_pause(); // hint to processor
		SleepEx(beats::min, TRUE); // lower cpu usage, yield to another thread

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

	SleepEx(beats::frame, FALSE); // yield to the threads
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
			SleepEx(beats::min, TRUE); // lower cpu usage, yield to another thread
		}
	}

}


#endif // ASYNC_LONG_TASK_IMPLEMENTATION
