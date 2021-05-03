#pragma	once
#include <Utility/class_helper.h>

template <class T>
class no_vtable tSingleton : no_copy					// alignment is specifically not set
{														// so that alignment can be specified by the contained class 
														// and its contained data instead ysing alignas(xx)

public:
	bool const IsNull() { return(nullptr == _instance); }

	__declspec(restrict) T* const __restrict Create();
	void Release();

	__inline __declspec(restrict) T * const __restrict Get() const {
		return(_instance);
	}

	__inline __declspec(restrict) T * const __restrict operator->() const {
		return(_instance);
	}
private:
	T* const __restrict _instance;

public:
	tSingleton();
	~tSingleton() = default;
	tSingleton & operator =(tSingleton const&) = delete;
	tSingleton(tSingleton const&) = delete;
};

template <class T>
tSingleton<T>::tSingleton()
	: _instance(nullptr)
{
}

template <class T>
__declspec(restrict) T* const __restrict tSingleton<T>::Create()
{
	static T single_instance;	// alignment specified by this class and its data is honored by using alignas(xx)

	const_cast<T* __restrict& __restrict>(_instance) = &single_instance;

	return(_instance);
}

template <class T>
void tSingleton<T>::Release()
{
	if (_instance) {

		T* const tmp(_instance);

		const_cast<T* __restrict& __restrict>(_instance) = nullptr;
		tmp->~T();
		
	}
}