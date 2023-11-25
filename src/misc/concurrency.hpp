#pragma once

#include <mutex>

namespace screenshare::misc {
	/**
	 * Represents a lock guard around a resource
	 */
	template<typename T>
	class ResourceLockGuard {
	private:
		T& mResource;
		std::mutex& mMutex;
	public:
		/**
		 * Locks the given resource using the given mutex
		 * @param resource The resource to obtain access to
		 * @param mutex The mutex
		 */
		ResourceLockGuard(T& resource, std::mutex& mutex)
			: mResource(resource), mMutex(mutex) {
			mMutex.lock();
		}

		~ResourceLockGuard() {
			mMutex.unlock();
		}

		ResourceLockGuard(const ResourceLockGuard&) = delete;
		ResourceLockGuard& operator=(const ResourceLockGuard&) = delete;

		/**
		 * Returns a reference to the resource
		 */
		T& get() {
			return mResource;
		}

		T* operator->() {
			return &mResource;
		}
	};

	/**
	 * Combines mutex with a resource
	 */
	template<typename T>
	class ResourceMutex {
	private:
		T mResource;
		std::mutex mMutex;

		/**
		 * Returns an unsafe (non locked) reference to the resource
		 */
		T& unsafeGet() {
			return mResource;
		}
	public:
		using Type = T;

		/**
		 * Creates a new mutex around the given mutex
		 */
		explicit ResourceMutex(T resource)
			: mResource(std::move(resource)) {

		}

		ResourceMutex(const ResourceMutex&) = delete;
		ResourceMutex& operator=(const ResourceMutex&) = delete;

		/**
		 * Returns a lock guard to access the resource
		 */
		ResourceLockGuard<T> guard() {
			return { mResource, mMutex };
		}

		template<typename... Ts>
		friend class ResourcesLockGuard;

		// Functions below to satisfy Lockable
		bool try_lock() {
			return mMutex.try_lock();
		}

		void lock() {
			mMutex.lock();
		}

		void unlock() {
			mMutex.unlock();
		}
	};
}