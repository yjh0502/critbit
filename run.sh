make bins;

echoerr() { echo "$@" 1>&2; }

export TIME="%E %M"
export LD_PRELOAD=/usr/lib/libjemalloc.so.1
for bin in `ls critbit.bin critbit_cow_stack.bin`;
do
    export FILENAME="${bin%.*}"_batch.out
    rm -f $FILENAME

    echoerr "$bin"
    for iter in 100000 333333 1000000 3333333 10000000;
    do
        ITER=$iter time ./$bin 2>&1 | tee -a $FILENAME
    done
done
