/*
 * VueceMemQueue.h
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#ifndef VUECEMEMQUEUE_H_
#define VUECEMEMQUEUE_H_

typedef struct VueceMemBulk
{
	void* data;
	struct VueceMemBulk *next;
	//the maximum size of this memory bulk
	int capacity;
	//the actual used size of the memory bulk
	int size_orginal;
//	int size_data;

	unsigned char *base;
	unsigned char *consume_start;
	unsigned char *end;

} VueceMemBulk;


class VueceMemQueue
{
public:
	VueceMemQueue();
	virtual ~VueceMemQueue();
	void Put(VueceMemBulk* m);
	VueceMemBulk* Peek();
	VueceMemBulk* Remove();
	bool IsEmpty();
	int Size();
	int BulkCount();
	int Read(uint8_t *buffer, int length);
	void FreeQueue();

	static VueceMemBulk* AllocMemBulk(int size);
	static void FreeMemBulk(VueceMemBulk* m);
	static int GetMemBulkActualDataSize(VueceMemBulk* m);


private:
	void InitQueue();
	static void InitMemBulk(VueceMemBulk* m);

private:
	VueceMemBulk* front;
	VueceMemBulk* rear;
	int size;
	int bulk_count;
};



#endif /* VUECEMEMQUEUE_H_ */
