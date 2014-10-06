
BINS='art.bin critbit.bin'
make $BINS

echoerr() { echo "$@" 1>&2; }

export TIME="%E %M"
for bin in $BINS;
do
    export FILENAME="${bin%.*}"_batch.out
    rm -f $FILENAME

    echoerr "$bin"
    for iter in 100000 333333 1000000 3333333 10000000 33333333 100000000;
    do
        ITER=$iter time ./$bin 2>&1 | tee -a $FILENAME
    done
done
