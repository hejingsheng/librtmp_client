#ifndef _AUTO_FREE_HPP
#define _AUTO_FREE_HPP

#include <stddef.h>

/**
 * To free the instance in the current scope, for instance, MyClass* ptr,
 * which is a ptr and this class will:
 *       1. free the ptr.
 *       2. set ptr to NULL.
 *
 * Usage:
 *       MyClass* po = new MyClass();
 *       // ...... use po
 *       AutoFree(MyClass, po);
 *
 * Usage for array:
 *      MyClass** pa = new MyClass*[size];
 *      // ....... use pa
 *      AutoFreeA(MyClass*, pa);
 *
 * @remark the MyClass can be basic type, for instance, AutoFreeA(char, pstr),
 *      where the char* pstr = new char[size].
 */
template<class T>
class impl_AutoFree
{
private:
	T** ptr;
	bool is_array;
public:
	impl_AutoFree(T** p, bool array) {
		ptr = p;
		is_array = array;
	}

	virtual ~impl_AutoFree() {
		if (ptr == NULL || *ptr == NULL) {
			return;
		}

		if (is_array) {
			delete[] * ptr;
		}
		else {
			delete *ptr;
		}

		*ptr = NULL;
	}
};

#define AutoFree(className, instance) \
impl_AutoFree<className> _auto_free_##instance(&instance, false)
#define AutoFreeA(className, instance) \
impl_AutoFree<className> _auto_free_array_##instance(&instance, true)

#endif
