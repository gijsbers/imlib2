#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "image.h"

/* attach a string key'd data and/or int value to an image that cna be */
/* looked up later by its string key */
__EXPORT__ void
__imlib_AttachTag(ImlibImage * im, const char *key, int val, void *data,
                  ImlibDataDestructorFunction destructor)
{
   ImlibImageTag      *t;

   /* no string key? abort */
   if (!key)
      return;

   /* if a tag of that name already exists - remove it and free it */
   if ((t = __imlib_RemoveTag(im, key)))
      __imlib_FreeTag(im, t);
   /* allocate the struct */
   t = malloc(sizeof(ImlibImageTag));
   /* fill it int */
   t->key = strdup(key);
   t->val = val;
   t->data = data;
   t->destructor = destructor;
   t->next = im->tags;
   /* prepend it to the list of tags */
   im->tags = t;
}

/* look up a tage by its key on the image it was attached to */
__EXPORT__ ImlibImageTag *
__imlib_GetTag(const ImlibImage * im, const char *key)
{
   ImlibImageTag      *t;

   t = im->tags;
   while (t)
     {
        if (!strcmp(t->key, key))
           return t;
        t = t->next;
     }
   /* no tag found - return NULL */
   return NULL;
}

/* remove a tag by looking it up by its key and removing it from */
/* the list of keys */
ImlibImageTag      *
__imlib_RemoveTag(ImlibImage * im, const char *key)
{
   ImlibImageTag      *t, *tt;

   tt = NULL;
   t = im->tags;
   while (t)
     {
        if (!strcmp(t->key, key))
          {
             if (tt)
                tt->next = t->next;
             else
                im->tags = t->next;
             return t;
          }
        tt = t;
        t = t->next;
     }
   /* no tag found - NULL */
   return NULL;
}

/* free the data struct for the tag and if a destructor function was */
/* provided call it on the data member */
void
__imlib_FreeTag(ImlibImage * im, ImlibImageTag * t)
{
   free(t->key);
   if (t->destructor)
      t->destructor(im, t->data);
   free(t);
}

/* free all the tags attached to an image */
void
__imlib_FreeAllTags(ImlibImage * im)
{
   ImlibImageTag      *t, *tt;

   t = im->tags;
   while (t)
     {
        tt = t;
        t = t->next;
        __imlib_FreeTag(im, tt);
     }
}
