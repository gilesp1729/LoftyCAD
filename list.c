#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>


// Link and unlink objects in a doubly linked list
void link(Object *new_obj, ListHead *obj_list)
{
    new_obj->next = obj_list->head;
    new_obj->prev = NULL;
    if (obj_list->head == NULL)
        obj_list->tail = new_obj;
    else
        obj_list->head->prev = new_obj;

    obj_list->head = new_obj;

    ASSERT(obj_list->head->prev == NULL, "First element should not have a prev");
    ASSERT(obj_list->tail->next == NULL, "Last element should not have a next");
}

void delink(Object *obj, ListHead *obj_list)
{
    if (obj->prev != NULL)
        obj->prev->next = obj->next;
    else
        obj_list->head = obj->next;

    if (obj->next != NULL)
        obj->next->prev = obj->prev;
    else
        obj_list->tail = obj->prev;

    ASSERT(obj_list->head == NULL || obj_list->head->prev == NULL, "First element should not have a prev");
    ASSERT(obj_list->tail == NULL || obj_list->tail->next == NULL, "Last element should not have a next");
}

void
link_tail(Object *new_obj, ListHead *obj_list)
{
    new_obj->next = NULL;
    if (obj_list->head == NULL)
    {
        obj_list->head = new_obj;
        new_obj->prev = NULL;
    }
    else
    {
        obj_list->tail->next = new_obj;
        new_obj->prev = obj_list->tail;
    }
    obj_list->tail = new_obj;

    ASSERT(obj_list->head->prev == NULL, "First element should not have a prev");
    ASSERT(obj_list->tail->next == NULL, "Last element should not have a next");
}

// Link and delink objects into a group object list
void link_group(Object *new_obj, Group *group)
{
    ASSERT(group->hdr.type == OBJ_GROUP, "Trying to link, but it's not a group");
    link(new_obj, &group->obj_list);
    new_obj->parent_group = group;
}

void delink_group(Object *obj, Group *group)
{
    ASSERT(group->hdr.type == OBJ_GROUP, "Trying to delink, but it's not a group");
    ASSERT(obj->parent_group == group, "Delinking from wrong group");
    delink(obj, &group->obj_list);
}

void link_tail_group(Object *new_obj, Group *group)
{
    ASSERT(group->hdr.type == OBJ_GROUP, "Trying to link, but it's not a group");
    link_tail(new_obj, &group->obj_list);
    new_obj->parent_group = group;
}

// Link objects into a singly linked list, chained through the next pointer.
// the prev pointer is used to point to the object, so it doesn't interfere
// with whatever list the object is actually participating in.
void link_single(Object *new_obj, ListHead *obj_list)
{
    Object *list_obj = obj_new();

    list_obj->next = obj_list->head;
    if (obj_list->head == NULL)
        obj_list->tail = list_obj;
    obj_list->head = list_obj;
    list_obj->prev = new_obj;
}

// Like link_single, but first checks if the object is already in the list.
void link_single_checked(Object *new_obj, ListHead *obj_list)
{
    Object *o;

    for (o = obj_list->head; o != NULL; o = o->next)
    {
        if (o->prev == new_obj)
            return;
    }
    link_single(new_obj, obj_list);
}

// Clean out a view list (a singly linked list of Points) by joining it to the free list.
// The points already have ID's of 0. 
void
free_point_list(ListHead *pt_list)
{
    if (pt_list->head == NULL)
        return;

    if (free_list_pt.head == NULL)
    {
        ASSERT(free_list_pt.tail == NULL, "Tail should be NULL");
        free_list_pt.head = pt_list->head;
        free_list_pt.tail = pt_list->tail;
    }
    else
    {
        ASSERT(free_list_pt.tail != NULL, "Tail should not be NULL");
        free_list_pt.tail->next = pt_list->head;
        free_list_pt.tail = pt_list->tail;
    }

    pt_list->head = NULL;
    pt_list->tail = NULL;
}

// Free all elements in a singly linked list of Objects, similarly to the above.
void free_obj_list(ListHead *obj_list)
{
    if (obj_list->head == NULL)
        return;

    if (free_list_obj.head == NULL)
    {
        ASSERT(free_list_obj.tail == NULL, "Tail should be NULL");
        free_list_obj.head = obj_list->head;
        free_list_obj.tail = obj_list->tail;
    }
    else
    {
        ASSERT(free_list_obj.tail != NULL, "Tail should not be NULL");
        free_list_obj.tail->next = obj_list->head;
        free_list_obj.tail = obj_list->tail;
    }

    obj_list->head = NULL;
    obj_list->tail = NULL;
}

// Allocate the point bucket list structure.
Point ***
init_buckets(void)
{
    Point ***bl = calloc(n_buckets, sizeof(Point **));
    int i;

    for (i = 0; i < n_buckets; i++)
        bl[i] = calloc(n_buckets, sizeof(Point *));

    return bl;
}

// Find a bucket header for a given point. The buckets are bottom-inclusive, top-exclusive in X and Y.
Point **
find_bucket(Point *p, Point ***bucket)
{
    int bias = n_buckets / 2;
    int i = (int)floor(p->x / bucket_size) + bias;
    int j = (int)floor(p->y / bucket_size) + bias;
    Point **bh;

    if (i < 0)
        i = 0;
    else if (i >= n_buckets)
        i = n_buckets - 1;
    bh = bucket[i];

    if (j < 0)
        j = 0;
    else if (j >= n_buckets)
        j = n_buckets - 1;

    return &bh[j];
}

// Clear a bucket structure to empty, but don't free anything.
void empty_bucket(Point ***bucket)
{
    int i, j;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
            bh[j] = NULL;
    }
}

// Free all the Points a bucket references, and clear the buckets to empty.
void free_bucket_points(Point ***bucket)
{
    int i, j;
    Point *p, *nextp;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
        {
            for (p = bh[j]; p != NULL; p = nextp)
            {
                nextp = p->bucket_next;
                p->hdr.next = free_list_pt.head;
                if (free_list_pt.head == NULL)
                    free_list_pt.tail = (Object *)p;
                free_list_pt.head = (Object *)p;
            }
            bh[j] = NULL;
        }
    }
}

// Free a bucket structure, and all the Points it references.
void free_bucket(Point ***bucket)
{
    int i, j;
    Point *p, *nextp;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
        {
            for (p = bh[j]; p != NULL; p = nextp)
            {
                nextp = p->bucket_next;
                p->hdr.next = free_list_pt.head;
                if (free_list_pt.head == NULL)
                    free_list_pt.tail = (Object *)p;
                free_list_pt.head = (Object *)p;
            }
        }
        free(bucket[i]);
    }
    free(bucket);
}
