#!/bin/sh

IMAGE=hdd.img
SIZE=308224       # size of the image in sectors, 512/sector = 150MB

dd if=/dev/zero of=$IMAGE bs=512 count=$SIZE

# 2048*512 = 1M reserved for grub, first partition starts at 1M, aka /boot
echo -e "o\nn\np\n1\n2048\n204800\na\nn\np\n2\n204801\n\nw" | sudo fdisk $IMAGE

#echo -e "o\nn\np\n1\n2048\n\nw" | sudo fdisk $IMAGE

# mount it twice

# at offset zero, the beginning of the disk, to write grub in first MB
sudo losetup /dev/loop0 $IMAGE

# mount it at offset 1M, aka first partition, to store /boot
sudo losetup /dev/loop1 -o 1048576 $IMAGE

# format the partition
sudo mke2fs /dev/loop1

## from sector 204801 -> will be COFS formated partition, aka /root

mkdir mnt
sudo mount /dev/loop1 mnt
sudo mkdir -p mnt/boot/grub
sudo cp kernel.bin mnt/boot/kernel

echo "(hd0,msdos1) /dev/loop0" > device.map

sudo grub-install --grub-mkdevicemap=device.map --root-directory=mnt --boot-directory=mnt/boot  --no-floppy --modules="normal part_msdos ext2 multiboot configfile biosdisk" /dev/loop0

cat << EOM | sudo tee mnt/boot/grub/grub.cfg
set timeout=3
set default=0 # Set the default menu entry 
menuentry "cOSiris" {
   multiboot /boot/kernel
   # module /boot/initrd.img
   boot
}
EOM


sync

sudo umount -l mnt
sudo losetup -d /dev/loop0
sudo losetup -d /dev/loop1
rm device.map
rm -rf mnt

#rm sda.vdi
#
# If you want to convert the raw image to a virtualbox disk,
# uncomment this
#VBoxManage convertdd hdd.img sda.vdi --format VDI
