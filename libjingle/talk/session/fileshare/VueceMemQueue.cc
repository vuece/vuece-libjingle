/*
 * VueceMemQueue.cc
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#include <stdlib.h>

#include "VueceLogger.h"
#include "VueceMemQueue.h"
#include "VueceConstants.h"

#ifndef MIN
#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#endif
#ifndef MAX
#define MAX(X, Y) ((X)>(Y)?(X):(Y))
#endif

VueceMemQueue::VueceMemQueue()
{
	//VueceLogger::Debug("VueceMemQueue - Constructor called");
	front = NULL;
	rear = NULL;
	size = 0;
	bulk_count = 0;
}

void VueceMemQueue::InitQueue()
{
	front = NULL;
	rear = NULL;
	size = 0;
	bulk_count = 0;
}

VueceMemQueue::~VueceMemQueue()
{
	//VueceLogger::Debug("VueceMemQueue - Destructor called");
}

bool VueceMemQueue::IsEmpty()
{
	if (rear == NULL)
		return true;
	return false;
}

int VueceMemQueue::Size()
{
	return size;
}


int VueceMemQueue::BulkCount()
{
	return bulk_count;
}



void VueceMemQueue::InitMemBulk(VueceMemBulk* m)

{
	if(m == NULL)
	{
		return;
	}

	m->data = NULL;
	m->next = NULL;
	m->capacity = 0;
	m->size_orginal = 0;
}

VueceMemBulk* VueceMemQueue::AllocMemBulk(int capacity)
{
	//VueceLogger::Debug("VueceMemQueue - Alloc with capacity: %d", capacity);

	VueceMemBulk* m = (VueceMemBulk*)malloc(sizeof(VueceMemBulk));

	if(m == NULL)
	{
		VueceLogger::Error("VueceMemQueue - Alloc failed, return null");
		return NULL;
	}

	InitMemBulk(m);

	m->data = malloc(capacity);

	if(m->data == NULL)
	{
		VueceLogger::Error("VueceMemQueue - Cannot allocate memory with size: %d, return null", capacity);
		return NULL;
	}

	m->capacity = capacity;

	m->end = m->consume_start = m->base = (unsigned char *)m->data;

	return m;
}

void VueceMemQueue::FreeMemBulk(VueceMemBulk* m)
{
	//VueceLogger::Debug("VueceMemQueue::FreeMemBulk - capacity: %d, size: %d", m->capacity, m->size);

	if( m == NULL)
	{
		return;
	}

	if(m->data != NULL)
		free(m->data);

	free(m);
}

int VueceMemQueue::GetMemBulkActualDataSize(VueceMemBulk* m)
{
	return (int)(m->end - m->consume_start);
}



void VueceMemQueue::Put(VueceMemBulk* m)
{
	//VueceLogger::Debug("VueceMemQueue::Put - Bulk capacity = %d, size = %d", m->capacity, m->size);

	if(IsEmpty())
	{
		rear = m;
		front = m;
	}
	else
	{
		rear->next = m;
		rear = m;
	}

	size+=m->size_orginal;

	bulk_count++;
}

VueceMemBulk* VueceMemQueue::Peek()
{
	//VueceLogger::Debug("VueceMemQueue::Peek");

	return front;
}

VueceMemBulk* VueceMemQueue::Remove()
{
	VueceMemBulk* m = NULL;

	//VueceLogger::Debug("VueceMemQueue::Remove");

	m = front;

	if(front == rear)
	{
		//VueceLogger::Debug("VueceMemQueue::Remove - Queue is empty now.");

		InitQueue();
	}
	else
	{
		front = m->next;

		size -= GetMemBulkActualDataSize(m);

		bulk_count--;
	}

	return m;
}


void VueceMemQueue::FreeQueue()
{
	VueceMemBulk* m = NULL;

	//VueceLogger::Debug("VueceMemQueue::FreeQueue START - Current bulk number: %d, size: %d", BulkCount(), Size());

	while ( (m = Remove()) != NULL)
	{
		FreeMemBulk(m);
	}

	//VueceLogger::Debug("VueceMemQueue::FreeQueue DONE - Current bulk number: %d, size: %d", BulkCount(), Size());
}



int VueceMemQueue::Read(uint8_t *data, int target_len)
{
	int sz=0;
	int cplen;
	int expected_remaining_len = 0;

	expected_remaining_len = size - target_len;

//	VueceLogger::Debug("VueceMemQueue::Read - target target_len = %d, current queue size = %d, expected_remaining_len = %d", target_len, size, expected_remaining_len);

	if(size < target_len)
	{
		VueceLogger::Fatal("VueceMemQueue::Read - Queue size is too small, sth is wrong");
		return 0;
	}

	VueceMemBulk* m = Peek();
	if(m == NULL)
	{
		VueceLogger::Warn("Queue is empty");
		return 0;
	}

	while(sz < target_len)
	{

		cplen = MIN(GetMemBulkActualDataSize(m), target_len-sz);

		memcpy(data+sz, m->consume_start, cplen);

		sz+=cplen;

		m->consume_start+=cplen;

		//data consumed, update the whole queue size
		size -= cplen;

		if(m->consume_start == m->end)
		{
			//VueceLogger::Debug("VueceMemQueue::Read - All data consumed in this bulk");

			//Note the value of 'size' is decreased in Remove() method
			m = Remove();
			VueceMemQueue::FreeMemBulk(m);

			m = Peek();
		}
	}

//	VueceLogger::Debug("VueceMemQueue::Read - Done, current size = %d", size);

	if(size != expected_remaining_len)
	{
		VueceLogger::Fatal("VueceMemQueue::Read - Size error, expected size = %d, actual size: %d", expected_remaining_len, size);
	}

	return target_len;
}

