#pragma once
#ifndef _H_ATTRIBUTE_
#define _H_ATTRIBUTE_

#include <vector>

template<typename T>
class Attribute {
protected:
	T const*     mBound;
	unsigned int mCount;
private:
	Attribute(const Attribute& other);
	Attribute& operator=(const Attribute& other);
public:
	Attribute();
	~Attribute();

	void Set(T const* inputArray, unsigned int arrayLength);
	void Set(std::vector<T> const& input);

	T const* const Get() const { return(mBound); }
	uint32_t const Count() const { return(mCount); }
};

template<typename T>
Attribute<T>::Attribute() {
	mCount = 0;
}

template<typename T>
Attribute<T>::~Attribute() {
}

template<typename T>
void Attribute<T>::Set(T const* inputArray, unsigned int arrayLength) {
	mCount = arrayLength;
	mBound = inputArray;
}

template<typename T>
void Attribute<T>::Set(std::vector<T> const& input) {
	Set(&input[0], (unsigned int)input.size());
}
#endif

