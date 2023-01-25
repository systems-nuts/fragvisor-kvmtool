
target="/tmp/tmp_ramdisk"
mkdir -p $target

folders="
bin
boot
etc
home
lib
lib64
mnt
opt
root
run
sbin
share
tmp
usr
var
"
if [[ $VM_IP == "" ]]; then
	echo "plz set \$VM_IP"
	exit -1
fi

echo -e "\n\nsave to $target\n\n"
for f in $folders; do
	echo "cpy $f"
	scp -r root@${VM_IP}:/$f $target
done
#sys
#lost+found
#media
#init
#init2
#kmeans
#ep-ao0-1t-argv
#ep-ao0-cpu0
#ep-ao0-cpu1
#ep-ao0-cpu2
#grep
#dev
#ffinity
#srv
#proc
