#include <gtest/gtest.h>

#include "test.h"

int                 debug = 0;

int
main(int argc, char **argv)
{
   const char         *s;

   ::testing::InitGoogleTest(&argc, argv);

   for (argc--, argv++; argc > 0; argc--, argv++)
     {
        s = argv[0];
        if (*s++ != '-')
           break;
        switch (*s)
          {
          case 'd':
             debug++;
             break;
          }
     }

   // Required by some tests
   mkdir(IMG_GEN, 0755);

   return RUN_ALL_TESTS();
}
