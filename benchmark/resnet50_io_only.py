import hdmlp
import hdmlp.lib.torch
import h5py as h5
import os
import comm

def main():

    #init distributed training
    comm_local_group = comm.init("nccl-slurm-pmi", 1)
    #comm_local_group = comm.init("gloo", 1)
    comm_rank = comm.get_rank()
    comm_local_rank = comm.get_local_rank()
    comm_size = comm.get_size()
    comm_local_size = comm.get_local_size()

    print("rank %d local_rank %d size %d local_size %d"%(comm_rank, comm_local_rank, comm_size, comm_local_size))

    # hdmlp_config_path = "/homes/kwf5687/dl_shuffling/NoPFS_codes/libhdmlp/data/ecp.cfg"
    hdmlp_config_path = "/global/homes/k/kwf5687/shuffling/dl_shuffling_813/NoPFS_codes/libhdmlp/data/perlmutter.cfg"
    # hdmlp_lib_path="/homes/kwf5687/dl_shuffling/NoPFS_codes/libhdmlp/build/libhdmlp.so"
    # hdmlp_lib_path="/global/homes/k/kwf5687/HDMLP/lib/python3.9/site-packages/hdmlp-0.1-py3.9.egg/libhdmlp.so"
    hdmlp_lib_path="/global/homes/k/kwf5687/shuffling/dl_shuffling_813/NoPFS_codes/libhdmlp/build/libhdmlp.so"
    # data_dir = "/files2/scratch/kwf5687/imagenet"
    data_dir = "/pscratch/sd/k/kwf5687/imagenet/imagenet/"
    #data_dir = "/pscratch/sd/k/kwf5687/imagenet/mini/"
    data_dir = os.path.join(data_dir, 'train/')
    print("init job")
    hdmlp_train_job = hdmlp.Job(
        data_dir,
        16*16,
        3,
        'uniform',
        False,
        transforms=[
            hdmlp.lib.transforms.ImgDecode(),
            hdmlp.lib.transforms.RandomResizedCrop(224),
            hdmlp.lib.transforms.RandomHorizontalFlip(),
            hdmlp.lib.transforms.HWCtoCHW()],
        seed=333,
        config_path=hdmlp_config_path,
        libhdmlp_path=hdmlp_lib_path)
    print("init train_dataset")
    train_dataset = hdmlp.lib.torch.HDMLPImageFolder(
        data_dir,
        hdmlp_train_job,
        filelist=os.path.join(data_dir, 'hdmlp_files.pickle'))
        # filelist=None)
    print(train_dataset.classes)
    print(train_dataset.class_to_idx)
    
    train_loader = hdmlp.lib.torch.HDMLPDataLoader(
        train_dataset)
    
    print(len(train_dataset))

if __name__ == "__main__":
    
    #run the stuff
    main()

    # Skip teardown to avoid hangs
    os._exit(0)