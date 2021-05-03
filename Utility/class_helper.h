#pragma once

// usage: class no_vtable cXXX 
#define no_vtable __declspec(novtable)

//! Base class for types that should not be assigned.
class no_assign {
	// Deny assignment
	no_assign& operator=(const no_assign&) = delete;
public:
	// allow default ctor
	no_assign() = default;
};

//! Base class for types that should not be copied or assigned.
class no_copy : no_assign {
	//! Deny copy construction
	no_copy(const no_copy&) = delete;
public:
	// allow default ctor
	no_copy() = default;
};


