make bins

export TIME="%E %M"
for bin in `ls *.bin`;
do
    export FILENAME="${bin%.*}".out
    rm -f $FILENAME

    for iter in 100000 333333 1000000 3333333 10000000;
    do
        ITER=$iter time ./$bin 2>&1 | tee -a $FILENAME
        ITER=$iter RAND=1 time ./$bin 2>&1 | tee -a $FILENAME
    done
done
