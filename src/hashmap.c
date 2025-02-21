#include "hashmap.h"
#include "hash.h"

#include <string.h>

#define HT_EXPANSION_FACTOR 2
#define HT_PRIME 37

/**
* Computes a hash value for a given string `str` by first applying an FNV-1a hash
* and then compressing the result.
*
* @param str The string to be hashed. A null pointer returns 0.
* @param rounds The number of iterations or rounds in hash compression
* @param m The size of the hash table. The result will lie within [0, m-1].
*
* @return A final hash value in the range [0, m-1]. If `str` is NULL, 0 is returned.
**/
size_t hash_key(const char* str, const size_t rounds, const size_t m) {
    if (str == NULL) return 0;

    size_t const k = fnv32a_hash_str(str);
    size_t const h1 = k % m;                             // 1st hash compression function
    size_t const h2 = HT_PRIME + (k % (m - HT_PRIME));   // 2nd hash compression function

    return (h1 + (rounds * h2)) % m;
}


// Definition of the hash table and its nodes
enum HashSlotMarker { EMPTY, INSERTED, DELETED };

typedef struct {
    const char* key;
    void* value;
    enum HashSlotMarker state;
} HashSlot;

struct HashMap {
    HashSlot* slots;
    size_t size;
    size_t capacity;
};

HashMap* hashmap_create(const size_t capacity) {
    // Allocate a new hash table on the stack
    HashMap* ht = malloc(sizeof(HashMap));
    ht->size = 0;
    ht->capacity = capacity;

    // Allocate the nodes for the initial capacity
    ht->slots = calloc(capacity, sizeof(HashSlot));
    if (ht->slots == NULL) {
        free(ht);
        return NULL;
    }
    return ht;
}

void hashmap_free(HashMap* hm) {
    for (size_t i = 0; i < hm->capacity; i++) {
        free((void*)hm->slots[i].key);
    }

    free(hm->slots);
    free(hm);
}

static const char* hashtable_slot_insert(HashSlot* slots, const size_t capacity,
                                         const char* key, void* value, const int cpy) {
    if (slots == NULL) return NULL;

    // Calculate the hashed index of the table
    size_t index = hash_key(key, 0, capacity);

    // Loop through the slots until we find an empty index (or a
    // matching key). It is guaranteed that one of these will occur,
    // since we have expanded the table as needed.
    size_t checks = 0;
    while (slots[index].key != NULL && slots[index].state != EMPTY) {
        // If a matching key is found at the index, we just need
        // to update its value and we're done!
        if (strcmp(slots[index].key, key) == 0) {
            slots[index].value = value;
            return slots[index].key;
        }

        // Otherwise, the key wasn't in this slot; we need to move
        // to the next index, as given by the checks
        checks++;
        index = hash_key(key, checks, capacity);
    }

    // We found an empty/deleted space, so create a new item at the free slot
    if (cpy == 0) {
        key = strdup(key);
        if (key == NULL) return NULL;
    }

    slots[index].key = (char*)key;
    slots[index].value = value;
    slots[index].state = INSERTED;

    return key;
}


/**
 * Expands the hash table by reallocating memory for a larger table.
 * Rehashes all existing keys into the new table.
 *
 * @param hm The hash table to be expanded.
 * @return The new capacity of the hash table.
 */
size_t hashtable_expand(HashMap* hm) {
    size_t const new_capacity = hm->capacity * HT_EXPANSION_FACTOR;
    HashSlot* new_slots = calloc(new_capacity, sizeof(HashSlot));

    // Rehash all existing keys into the new table
    for (size_t i = 0; i < hm->capacity; i++) {
        HashSlot const slot = hm->slots[i];

        if (slot.state == INSERTED) {
            hashtable_slot_insert(new_slots, new_capacity, slot.key, slot.value, 1);
        }
    }

    // Ensure we're freeing old slots
    free(hm->slots);

    // Update hash table's slots and capacity
    hm->slots = new_slots;
    hm->capacity = new_capacity;

    return new_capacity;
}

const char* hashmap_insert(HashMap* hm, const char* key, void* value) {
    if (hm->slots == NULL || value == NULL) return NULL;

    // Check if we first need to expand the table, doing so if needed
    size_t capacity = hm->capacity;
    if (hm->size >= (capacity / HT_EXPANSION_FACTOR)) {
        capacity = hashtable_expand(hm);
    }

    const char* result = hashtable_slot_insert(hm->slots, capacity, key, value, 0);
    if (result == NULL) return NULL;

    hm->size++;
    return result;
}


int hashmap_remove(HashMap* hm, const char* key) {
    // Calculate the hashed index of the table
    size_t index = hash_key(key, 0, hm->capacity);

    // Loop through the slots until we find an empty index, which
    // would mean that there is no such item in the list
    size_t checks = 0;
    while (hm->slots[index].key != NULL && hm->slots[index].state != EMPTY) {
        // If a matching key is found at the index, we should now delete it
        if (strcmp(hm->slots[index].key, key) == 0) {
            hm->slots[index].state = DELETED;
            hm->slots[index].value = NULL;
            hm->slots[index].key = NULL;

            hm->size--;
            return 1;
        }

        // Otherwise, the key wasn't in this slot; we need to check
        // the next index, to check a collisions
        checks++;
        index = hash_key(key, checks, hm->capacity);
    }

    // There is no such item in the hash table, so it cannot be deleted
    return 0;
}

void* hashmap_get(const HashMap* hm, const char* key) {
    if (key == NULL) return NULL;

    size_t index = hash_key(key,0, hm->capacity);
    size_t checks = 0;
    while (hm->slots[index].key != NULL && hm->slots[index].state != EMPTY) {
        // If we have a matching key, return it
        if (strcmp(hm->slots[index].key, key) == 0) {
            return hm->slots[index].value;
        }

        // Otherwise, check the next possible index
        checks++;
        index = hash_key(key, checks, hm->capacity);
    }

    // There is no such item in the hashtable
    return NULL;
}

