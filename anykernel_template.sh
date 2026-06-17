### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=KSU Next OP6893 COS16 by xCaptaiN09
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=RMX3031
device.name2=Realme X7 Max
device.name3=cupida
device.name4=mt6893
device.name5=DN2101
device.name6=DN2103
device.name7=denniz
device.name8=OnePlus Nord 2
device.name9=OnePlusNord2
device.name10=RealmeX7Max
device.name11=op6893
device.name12=OP6893
device.name13=RMX3350
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

ui_print " ";
ui_print "╔════════════════════════════════════════╗";
ui_print "║                                        ║";
ui_print "║            KSU NEXT KERNEL             ║";
ui_print "║          OP6893 | COS16/RUI/OOS        ║";
ui_print "║                                        ║";
ui_print "╠════════════════════════════════════════╣";
ui_print "║                                        ║";
ui_print "║  Maintainer  xCaptaiN09                ║";
ui_print "║  Base        OPlus MT6893 4.19         ║";
ui_print "║  Target      Realme UI / ColorOS / OOS ║";
ui_print "║  Root        KernelSU Next             ║";
ui_print "║  SuSFS       v2.1.0                    ║";
ui_print "║  KPM         External/kpatch           ║";
ui_print "║                                        ║";
ui_print "╠════════════════════════════════════════╣";
ui_print "║                                        ║";
ui_print "║  Realme X7 Max 5G    RMX3031/cupida    ║";
ui_print "║  OnePlus Nord 2      DN2101/denniz     ║";
ui_print "║                                        ║";
ui_print "╚════════════════════════════════════════╝";
ui_print " ";

# boot install
ui_print "  [1/2] Extracting boot image...";
split_boot;

# init.rc patch for Color OS 16 / Realme UI cgroup fix
ui_print "  Applying Realme UI / ColorOS cgroup patch...";
backup_file init.rc;
replace_string init.rc "cpuctl cpu,timer_slack" "mount cgroup none /dev/cpuctl cpu" "mount cgroup none /dev/cpuctl cpu,timer_slack";

ui_print "  [2/2] Flashing Image.gz-dtb...";
flash_boot;

ui_print " ";
ui_print "  Installation complete.";
ui_print " ";
## end boot install
