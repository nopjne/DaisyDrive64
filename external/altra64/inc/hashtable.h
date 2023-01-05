//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2011 @marekweb https://github.com/marekweb/datastructs-c
// See LICENSE file in the project root for full license information.
//

#ifndef _HASHTABLE_H
#define	_HASHTABLE_H

typedef struct hashtable hashtable;
void hashtable_destroy(hashtable *t);
typedef struct hashtable_entry hashtable_entry;
hashtable_entry *hashtable_body_allocate(unsigned int capacity);
hashtable *hashtable_create();
void hashtable_remove(hashtable *t,char *key);
void hashtable_resize(hashtable *t,unsigned int capacity);
void hashtable_set(hashtable *t,char *key,void *value);
void *hashtable_get(hashtable *t,char *key);
unsigned int hashtable_find_slot(hashtable *t,char *key);
unsigned long hashtable_hash(char *str);
struct hashtable {
	unsigned int size;
	unsigned int capacity;
	hashtable_entry* body;
};
struct hashtable_entry {
	char* key;
	void* value;
};

#endif
