// Inroduction to hash tables in C

#include <inttypes.h> // for portable integer declarations
#include <stdbool.h>  // for bool type
#include <stdio.h>    // for printf()
#include <stdlib.h>   // for malloc(), free(), NULL
#include <string.h>   // for strcmp()

#include "hashtable.h"

void item_print(item_t *i)
{
    printf("key=%s value=%d", i->key, i->value);
}

// Initialise a new hash table with n buckets.
// pre: true
// post: (return == false AND allocation of table failed)
//       OR (all buckets are null pointers)
bool htab_init(htab_t *h, size_t n)
{
    h->buckets = (item_t **)malloc(n * sizeof(item_t));
    h->size = n;
    for (size_t i = 0; i < n; i++)
    {
        h->buckets[i] = NULL;
    }
    return true;
}

// The Bernstein hash function.
// A very fast hash function that works well in practice.
size_t djb_hash(char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0')
    {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Calculate the offset for the bucket for key in hash table.
size_t htab_index(htab_t *h, char *key)
{
    return djb_hash(key) % h->size;
}

// Find pointer to head of list for key in hash table.
item_t *htab_bucket(htab_t *h, char *key)
{
    return h->buckets[htab_index(h, key)];
}

// Find an item for key in hash table.
// pre: true
// post: (return == NULL AND item not found)
//       OR (strcmp(return->key, key) == 0)
item_t *htab_find(htab_t *h, char *key)
{
    for (item_t *head = htab_bucket(h, key); head != NULL; head = head->next)
    {
        if (strcmp(head->key, key) == 0)
        {
            return head;
        }
    }
    printf("Item not found\n");
    return NULL;
}

// Add a key with value to the hash table.
// pre: htab_find(h, key) == NULL
// post: (return == false AND allocation of new item failed)
//       OR (htab_find(h, key) != NULL)
bool htab_add(htab_t *h, char *key, int value)
{
    item_t *head = htab_bucket(h, key);

    item_t *new_head = (item_t *)malloc(sizeof(item_t));
    new_head->key = key;
    new_head->value = value;
    new_head->next = head;

    h->buckets[htab_index(h, key)] = new_head;

    return true;
}

// Print the hash table.
// pre: true
// post: hash table is printed to screen
void htab_print(htab_t *h)
{
    printf("hash table with %ld buckets\n", h->size);
    for (size_t i = 0; i < h->size; ++i)
    {
        printf("bucket %ld: ", i);
        if (h->buckets[i] == NULL)
        {
            printf("empty\n");
        }
        else
        {
            for (item_t *j = h->buckets[i]; j != NULL; j = j->next)
            {
                item_print(j);
                if (j->next != NULL)
                {
                    printf(" -> ");
                }
            }
            printf("\n");
        }
    }
}

// Delete an item with key from the hash table.
// pre: htab_find(h, key) != NULL
// post: htab_find(h, key) == NULL
void htab_delete(htab_t *h, char *key)
{
    item_t *head = htab_bucket(h, key);
    item_t *current = head;
    item_t *previous = NULL;
    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            if (previous == NULL)
            { // first item in list
                h->buckets[htab_index(h, key)] = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            break;
        }
        previous = current;
        current = current->next;
    }
}

// Destroy an initialised hash table.
// pre: htab_init(h)
// post: all memory for hash table is released
void htab_destroy(htab_t *h)
{
    for (size_t i = 0; i < h->size; i++)
    {
        item_t *head = h->buckets[i];
        item_t *temp = head;
        while (temp != NULL)
        {
            temp = temp->next;
            free(head);
        }
    }
}
/*
int main(int argc, char **argv)
{
    // create a hash table with 10 buckets
    printf("creating hash table:\n");
    size_t buckets = 10;
    htab_t h;
    if (!htab_init(&h, buckets))
    {
        printf("failed to initialise hash table\n");
        return EXIT_FAILURE;
    }

    // add items to hash table and preint
    htab_add(&h, "hello", 1);
    htab_add(&h, "hello", 1); // violate pre-condition to get two hello's in table
    htab_add(&h, "world", 2);
    htab_print(&h);
    printf("\n");

    // find item in hash table
    printf("find item in hash table:\n");
    item_print(htab_find(&h, "hello"));
    printf("\n\n");

    // delete items from hash table and print
    printf("deleting items from hash table:\n");
    htab_delete(&h, "hello");
    htab_delete(&h, "world");
    htab_print(&h);
    printf("\n");

    // clean up hash table
    htab_destroy(&h);
}
*/
