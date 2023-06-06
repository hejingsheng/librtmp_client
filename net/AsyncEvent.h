#ifndef _ASYNC_EVENT_H_
#define _ASYNC_EVENT_H_

#include "uv.h"
#include <string>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>

namespace NetCore
{
	using AsyncCallback = std::function<void()>;

	class AsyncCore
	{
	public:
		AsyncCore(uv_loop_t *loop);
		virtual ~AsyncCore();

	public:
		void postAsyncEvent(AsyncCallback callback);
		void closeAsync();

	private:
		void process_callback();
		static void async_callback(uv_async_t* handle);

	private:
		uv_async_t *async_;
		std::mutex queueMutex_;
		std::queue<AsyncCallback> cbQueue_;
	};
}

#endif