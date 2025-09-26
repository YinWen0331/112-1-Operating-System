make distclean
make
timeout 1 ../build.linux/nachos -d z -ep hw4_normal_test1 0 -ep hw4_normal_test2 0 
timeout 1 ../build.linux/nachos -d z -ep hw4_normal_test1 50 -ep hw4_normal_test2 50 
timeout 1 ../build.linux/nachos -d z -ep hw4_normal_test1 50 -ep hw4_normal_test2 90 
timeout 1 ../build.linux/nachos -d z -ep hw4_normal_test1 70 -ep hw4_normal_test2 80 -ep hw4_normal_test3 50
timeout 1 ../build.linux/nachos -d z -ep hw4_normal_test1 100 -ep hw4_normal_test2 100
timeout 3 ../build.linux/nachos -d z -ep hw4_delay_test1 100 -ep hw4_normal_test2 100
timeout 3 ../build.linux/nachos -d z -ep hw4_delay_test1 50 -ep hw4_normal_test2 45

echo "done"






