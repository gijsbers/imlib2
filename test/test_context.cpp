#include <gtest/gtest.h>

#include <stddef.h>
#include <Imlib2.h>

#include "test.h"

#define NULC ((Imlib_Context*)0)

#define HAVE_INITIAL_CTX 1

TEST(CTX, ctx)
{
   Imlib_Context       ctx, ctx0, ctx1;

   // Part 1

   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
#if !HAVE_INITIAL_CTX
   EXPECT_EQ(ctx, NULC);

   // Delete NULL should just complain
   imlib_context_free(ctx);
#else
   EXPECT_TRUE(ctx);

   // Delete initial should just be ignored
   imlib_context_free(ctx);
#endif

   // Part 2

#if !HAVE_INITIAL_CTX
   ctx0 = imlib_context_new();  // Create initial context
   D("%d: ctx0 = %p\n", __LINE__, ctx0);
   EXPECT_TRUE(ctx0);
   // * NB! After first call to imlib_context_push() we cannot ged rid of
   //       the first context
   imlib_context_push(ctx0);

   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx is initial
#else
   ctx0 = imlib_context_get();
   D("%d: ctx0 = %p\n", __LINE__, ctx0);
   EXPECT_TRUE(ctx0);
#endif

   // Attempt to pop first (should not succeed)
   imlib_context_pop();
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx is still initial

   // Attempt to free first (should not succeed)
   imlib_context_free(ctx);
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx is still initial

   // Part 3

   ctx1 = imlib_context_new();
   D("%d: ctx1 = %p\n", __LINE__, ctx1);
   EXPECT_TRUE(ctx1);
   EXPECT_NE(ctx0, ctx1);

   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx still default

   imlib_context_push(ctx1);
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx1);        // Ctx now new

   // Attempt to free new current (should not succeed)
   imlib_context_free(ctx1);
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx1);        // Ctx still new

   imlib_context_pop();
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx now default

#if 0
   // No! - The previous delete set ->dirty which will have caused
   //       imlib_context_pop() above to free it.
   // Free new context, now unused
   imlib_context_free(ctx1);
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx still default
#endif

   imlib_context_pop();
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx still default

   // Free initial context (should not succeed)
   imlib_context_free(ctx0);
   ctx = imlib_context_get();
   D("%d: ctx  = %p\n", __LINE__, ctx);
   EXPECT_EQ(ctx, ctx0);        // Ctx still default
}
