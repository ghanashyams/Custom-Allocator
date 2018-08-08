// CustomAllocator.cpp : Defines the entry point for the console application.
//


#include "stdafx.h"
#include "windows.h"
#include <vector>
#include <map>
#include <allocators>
#include <iostream>


class MemoryAllocator
{
public:
	~MemoryAllocator();
	MemoryAllocator(UINT unit_size, UINT block_size = 16);
	MemoryAllocator(UINT unit_size, UINT block_size, bool debug);
	void* allocate();
	void deallocate(void*);
	bool  is_debug() const
	{
		return _debug;
	}
private:
	struct Free
	{
		Free* next;
	};
	UINT _requested_size;    // memory units requested for allocation
	UINT _allocated_size;    // memory units to be allocated
	UINT _block_size;        // The number of units in blocks 
	void** _allocated_blocks;    // Array of pointers to blocks
	UINT _num_allocated_blocks;  // Number of allocated blocks 
	Free* _free;                 // Linked list of free units
	bool _debug;

	void more() throw(char*);
	void* debug_check(void*&) throw(char*);
	void* debug_correct(void*&);
};

inline
void*
MemoryAllocator::allocate()
{
	if (!_free)
		more();
	void* storage = _free;
	_free = _free->next;
	return _debug ? debug_correct(storage) : storage;
}

inline
void
MemoryAllocator::deallocate(void* storage)
{
	if (_debug)
		debug_check(storage);
	Free* new_free = (Free*)storage;
	new_free->next = _free;
	_free = new_free;
}

#define TRAILING_ 0x55555555
#define HEADING_ 0x55555555

MemoryAllocator::MemoryAllocator(UINT unit_size, UINT block_size)
	: _allocated_size(max(unit_size, sizeof(Free))),
	_requested_size(unit_size),
	_block_size(block_size),
	_num_allocated_blocks(0),
	_allocated_blocks(0),
	_free(0),
	_debug(false)
{}

MemoryAllocator::MemoryAllocator(UINT unit_size, UINT block_size,
	bool debug)
	: _allocated_size(max(unit_size, sizeof(Free))),
	_requested_size(unit_size),
	_block_size(block_size),
	_num_allocated_blocks(0),
	_allocated_blocks(0),
	_free(0),
	_debug(debug)
{
	if (_debug)
		_allocated_size += 2 * sizeof(int);
}

MemoryAllocator::~MemoryAllocator()
{
	for (int k = 0; k < _num_allocated_blocks; ++k)
	{
		::operator delete(_allocated_blocks[k]);
	}
	::operator delete(_allocated_blocks);
}

void
MemoryAllocator::more() throw(char*)
{
	Free* new_block = (Free*)
		::operator new(_allocated_size *_block_size);
	void** new_blocks = (void**) ::operator new(sizeof(void*)
		* (_num_allocated_blocks + 1));
	int last_element = _block_size - 1;
	if (!new_block || !new_blocks)
		throw("Memory allocation failed.");
	if (_allocated_blocks)
	{
		memcpy(new_blocks, _allocated_blocks, sizeof(void*)
			* _num_allocated_blocks);
		::operator delete(_allocated_blocks);
	}
	_allocated_blocks = new_blocks;
	_allocated_blocks[_num_allocated_blocks++] = new_block;
	_free = new_block;

	for (int k = 0; k<last_element; ++k, new_block = new_block->next)
	{
		new_block->next = (Free*)((char*)new_block
			+ _allocated_size);
	}

	new_block->next = 0;
}

void*
MemoryAllocator::debug_correct(void*& storage)
{
	*(int*)storage = HEADING_;
	storage = (int*)storage + 1;
	*(int*)((char*)storage + _requested_size) = TRAILING_;
	return storage;
}

void*
MemoryAllocator::debug_check(void*& storage) throw(char*)
{
	int* tail = (int*)((char*)storage + _requested_size);
	int* head = (int*)(storage = (int*)storage - 1);

	if (*tail != TRAILING_)
		throw("Block tail has been overrun.");
	if (*head != HEADING_)
		throw("Block header has been overrun.");
	return storage;
}


template<class T>
class TheAllocator : public MemoryAllocator
{
public:
	TheAllocator(bool debug = false, UINT block_size = 16)
		: MemoryAllocator(sizeof(T), block_size, debug)
	{}
};

class Foo
{
public:    // the Foo specific stuff

	void* operator new (size_t)
	{
		return _allocator.allocate();
	}
	void operator delete (void* storage)
	{
		_allocator.deallocate(storage);
	}
	
private:    // more of the Foo stuff
	static TheAllocator<Foo> _allocator;
};

TheAllocator<Foo> Foo::_allocator;

namespace MyLib {
	template <class T>
	class MyAlloc {
	public:
		static TheAllocator<T> _allocator;

		// type definitions
		typedef T        value_type;
		typedef T*       pointer;
		typedef const T* const_pointer;
		typedef T&       reference;
		typedef const T& const_reference;
		typedef std::size_t    size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template <class U>
		struct rebind {
			typedef MyAlloc<U> other;
		};

		// return address of values
		pointer address(reference value) const {
			return &value;
		}
		const_pointer address(const_reference value) const {
			return &value;
		}

		/* constructors and destructor
		* - nothing to do because the allocator has no state
		*/
		MyAlloc() throw() {
		}

		MyAlloc(const MyAlloc&) throw() {
		}

		template <class U>
		MyAlloc(const MyAlloc<U>&) throw() {
		}

		~MyAlloc() throw() {
		}

		// allocate but don't initialize num elements of type T
		pointer allocate(size_type num, const void* = 0) {
			// print message and allocate memory with global new
			pointer ret;
#ifdef SYSTEM_ALLOC
			std::cerr << "allocate " << num << " element(s)"
				<< " of size " << sizeof(T) << std::endl;
			ret = (pointer)(::operator new(num * sizeof(T)));
			std::cerr << " allocated at: " << (void*)ret << std::endl;

#endif
			ret = (pointer)_allocator.allocate();
			return ret;
		}

		//// destroy elements of initialized storage p
		void destroy(pointer p) {
			// destroy objects by calling their destructor
			p->~T();
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type num) {
			// print message and deallocate memory with global delete
#ifdef SYSTEM_ALLOC
			std::cerr << "deallocate " << num << " element(s)"
			<< " of size " << sizeof(T)
			<< " at: " << (void*)p << std::endl;*/
			::operator delete((void*)p);
#endif
			_allocator.deallocate(p);
		}

	};

	// return that all specializations of this allocator are interchangeable
	template <class T1, class T2>
	bool operator== (const MyAlloc<T1>&,
		const MyAlloc<T2>&) throw() {
		return true;
	}
	template <class T1, class T2>
	bool operator!= (const MyAlloc<T1>&,
		const MyAlloc<T2>&) throw() {
		return false;
	}
}
template <class T>
TheAllocator<T> MyLib::MyAlloc<T>::_allocator;



int main()
{
	while (1)
	{
		{
			std::cout << "Hello" << std::endl;
			Sleep(2000);
			std::vector<Foo, MyLib::MyAlloc<Foo> > v;

			Foo pFoo2, pFoo3;
			v.push_back(pFoo2);
			v.push_back(pFoo3);

			//std::map<int, Foo> m_map;
			std::map<int, Foo, std::less<int>, MyLib::MyAlloc<std::pair<int, Foo>> > m_map;
			m_map[1] = pFoo2;
			m_map[2] = pFoo3;




		}
	}

    return 0;
}

