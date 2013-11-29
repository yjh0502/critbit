make bins

export TIME="%E %M"
for bin in `ls critbit.bin critbit_cow_stack.bin critbit_cow_loop.bin`;
do
    export FILENAME="${bin%.*}".out
    rm -f $FILENAME

    for iter in 100000 333333 1000000 3333333 10000000;
    do
        ITER=$iter time ./$bin 2>&1 | tee -a $FILENAME
        ITER=$iter RAND=1 LD_PRELOAD=/usr/lib/libjemalloc.so.1 time ./$bin 2>&1 | tee -a $FILENAME
    done
done
