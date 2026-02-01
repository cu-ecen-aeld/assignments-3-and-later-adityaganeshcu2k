#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
    OUTDIR=/tmp/aeld
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

# Convert to absolute path
OUTDIR=$(realpath ${OUTDIR})

# Create outdir if it doesn't exist
if ! mkdir -p "${OUTDIR}"; then
    echo "ERROR: Failed to create directory ${OUTDIR}"
    exit 1
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION} linux-stable
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Building the Linux kernel"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

fi

echo "Adding the Image in outdir"

cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs/{bin,sbin,etc,proc,sys,usr/{bin,sbin},dev,lib,lib64,home}


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox

cd "$OUTDIR/busybox"

# Clean any previous builds
make distclean

# Configure busybox
make defconfig

# Compile busybox
make CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)

# Install busybox to rootfs
make CONFIG_PREFIX=${OUTDIR}/rootfs CROSS_COMPILE=${CROSS_COMPILE} install

# Copy busybox to outdir/bin for readelf check
mkdir -p ${OUTDIR}/bin
cp ${OUTDIR}/rootfs/bin/busybox ${OUTDIR}/bin/

# Now check library dependencies
echo "Library dependencies"
INTERPRETER=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter" | awk '{print $NF}'  | sed 's/\/lib\///g; s/]//g')
SHARED_LIBS=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library" | awk '{print $NF}' | sed 's/\[\|\]//g')
echo "Program interpreter: $INTERPRETER"
echo "Shared libraries: $SHARED_LIBS"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

mkdir -p ${OUTDIR}/rootfs/lib
mkdir -p ${OUTDIR}/rootfs/lib64

cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1


# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer ${OUTDIR}/rootfs/home/
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
sed -i 's|\.\./conf/assignment.txt|conf/assignment.txt|g' \
    ${OUTDIR}/rootfs/home/finder-test.sh

    
# Copy autorun script
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/

# Ensure conf directory exists in rootfs home
mkdir -p ${OUTDIR}/rootfs/home/conf

# Copy all contents from host conf into rootfs home/conf
cp -r ${FINDER_APP_DIR}/conf/* ${OUTDIR}/rootfs/home/conf/


# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio

