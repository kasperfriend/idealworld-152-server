/*
	amemory.cpp - the out of line pieces of abase::fast_allocator, the small
	object allocator the gameserver routes its per object new/delete through
	(cgame/include/amemory.h and abase::ASmallObject).

	Everything else in that header is inline, so all the library has to
	supply is the bucket table and the three accounting counters that
	gs/start.cpp dumps every four seconds:

		_a_table[size]		lazy fix_allocator for allocations < 1024 bytes
		_inside_counter		count of blocks >= MAX_RECORD_SIZE
		_large_size_counter[]	count per size in [1024, 10240)
		_other_counter		count of raw malloc() blocks

	Both _Slock (the spinlock inside every bucket) and _Ap (the allocator)
	start out empty: fast_allocator::alloc() creates a fix_allocator for a
	size on first use while holding that bucket's lock.
*/

#include "amemory.h"

namespace abase
{

abase::fast_allocator::node_t abase::fast_allocator::_a_table[abase::fast_allocator::MAX_SIZE];

int abase::fast_allocator::_other_counter = 0;
int abase::fast_allocator::_inside_counter = 0;

int abase::fast_allocator::_large_size_counter[
	abase::fast_allocator::MAX_RECORD_SIZE - abase::fast_allocator::MAX_SIZE];

}
