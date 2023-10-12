#srun -n 1 python3 -u resnet50.py \
#        --data-dir "/pscratch/sd/k/kwf5687/imagenet/mini/" \
#        --dataset "imagenet" \
#        --batch-size 16 \
#        --epochs 4 \
#        --dist \
#        --hdmlp-config-path "/global/homes/t/tianle/myWork/wkw_cache/NoPFS/libhdmlp/data/perlmutter.cfg" \
#        --hdmlp-lib-path "/global/homes/t/tianle/myWork/wkw_cache/HDMLP/lib/python3.9/site-packages/hdmlp-0.1-py3.9.egg/libhdmlp.so" \
#        -r "gloo" \
#        --output-dir "/global/homes/t/tianle/myWork/wkw_cache/work/" \
#        --no-prefetch \
#        --print-freq 1000000 \
#        --no-eval \
#        --hdmlp

python3 -u resnet50.py \
        --data-dir "/pscratch/sd/k/kwf5687/imagenet/mini/" \
        --dataset "imagenet" \
        --batch-size 16 \
        --epochs 4 \
        --dist \
        --hdmlp-config-path "/global/homes/t/tianle/myWork/wkw_cache/NoPFS/libhdmlp/data/perlmutter.cfg" \
        --hdmlp-lib-path "/global/homes/t/tianle/myWork/wkw_cache/HDMLP/lib/python3.9/site-packages/hdmlp-0.1-py3.9.egg/libhdmlp.so" \
        -r "gloo" \
        --output-dir "/global/homes/t/tianle/myWork/wkw_cache/work/" \
        --no-prefetch \
        --print-freq 1000000 \
        --no-eval \
        --hdmlp
