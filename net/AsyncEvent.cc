#include "AsyncEvent.h"

namespace NetCore
{
	AsyncCore::AsyncCore(uv_loop_t *loop)
	{
		async_ = new uv_async_t();
		uv_async_init(loop, async_, &AsyncCore::async_callback);
		async_->data = this;
	}

	AsyncCore::~AsyncCore()
	{

	}

	void AsyncCore::postAsyncEvent(AsyncCallback callback)
	{
		queueMutex_.lock();
		cbQueue_.push(callback);
		queueMutex_.unlock();
		uv_async_send(async_);
	}

	void AsyncCore::closeAsync()
	{
		if (::uv_is_closing((uv_handle_t*)async_) == 0)
		{
			::uv_close((uv_handle_t*)async_, [](uv_handle_t* handle)
			{
				delete (uv_async_t*)handle;
			});
		}
	}

	void AsyncCore::process_callback()
	{
		queueMutex_.lock();
		std::queue<AsyncCallback> callbacks;
		callbacks.swap(this->cbQueue_);
		queueMutex_.unlock();
		while (!callbacks.empty())
		{
			auto cb_func = callbacks.front();
			cb_func();
			callbacks.pop();
		}
	}

	void AsyncCore::async_callback(uv_async_t* handle)
	{
		auto data = (AsyncCore*)handle->data;
		data->process_callback();
	}
}
