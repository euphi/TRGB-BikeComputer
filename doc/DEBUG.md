# DEBUG

## Debug functions of TRGB-Bikecomputer

* Event log
* Statistics Array
* Distance Details


## Logging

* TBD


## Core Dump

A core dump is a copy of stack and other memory in case of a crash, so it can later be used for investigation of the cause.

By default, Arduino ESP32 is configured so that core dumps are written to flash memory.

### How to get a coredump

1. Install ESP32 IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/
1. Activate IDF environment, in IDF install directory: (the leading dot is important - it means to execute the script and "sources" its output)

`. export.sh `

1. Connect your ESP32 to USB
1. Call espcoredump Utility with the elf file of your binary

`espcoredump.py  --port /dev/ttyACM0 info_corefile ~/Coding/Bike/TRGB-BikeComputer/.pio/build/trgb-esp32-s3/firmware.elf`

This downloads the core file via USB and analyses it using the firmware.elf file. The elf file is created with debug information, so you will see variable names, function names and references to source files.

This it is mandatory that the elf file corresponds to your installed binary file.

You can also use the command `debug_corefile` instead of `info_corefile`, which opens a debugger (gdb) instead of showing some info only. This is much more powerful, but needs experience with gdb. 


### Understand the core dump (Example)

The first information you get from a coredump is why and where the software crashed.

However, please be aware that the crash may be quite "far away" from the root cause. The ESP32 crashes only for illegal memory access, illegal instructions, critical runtime errors (e. g. integer division by zero) and if the software fails intentionally (`abort`), e.g. due to detected heap corruptions, stack overflow, sw watchdog or failed `assert()`.

Invalid pointers can be carried other several function calls without memory access or memory corruptions can even occur in another thread.

In the following example, a wrong array index leads for functions calls "below" to writing memory at illegal address


The `info_corefile` output starts with output from accessing and downloading flash from the ESP32 and the with information about the current registers.

Most important is the excause (0x1d StoreProhibitedCause - attempt to write to illegal memory adress) and the pc (adress of code where the illegal access is).

```

[...]

================== CURRENT THREAD REGISTERS ===================
exccause       0x1d (StoreProhibitedCause)
excvaddr       0x7e
epc1           0x420f0a91

[...]

pc             0x420223f2          0x420223f2 <lv_chart_set_x_start_point+46>
lbeg           0x40056f08          1074097928
lend           0x40056f12          1074097938
lcount         0x0                 0
sar            0x19                25
ps             0x60c20             396320
threadptr      <unavailable>

[...]

a15            0x3fcafa00          1070266880

```

Next is the backtrace of the current stack of the thread where the crash occured. This gives valuable information which functions where called before the crash and the values of parameters:

```

==================== CURRENT THREAD STACK =====================
#0  lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
#1  0x4200dd56 in ui_ScrChartSetPostFirst (pos=1, idx=<optimized out>) at src/ui/Screens/Chart/ui_Chart_CustFunc.c:19
#2  0x4200b5c8 in UIFacade::setChartPosFirst (this=0x3fca1750 <ui>, pos=1, idx=149 '\\225') at src/UIFacade.cpp:308
#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411
#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112
#5  0x4200a7f0 in Statistics::<lambda(Statistics*)>::operator() (__closure=0x0, thisInstance=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:50
#6  Statistics::<lambda(Statistics*)>::_FUN(Statistics *) () at src/Stats/Statistics.cpp:50
#7  0x42086a11 in timer_process_alarm (dispatch_method=ESP_TIMER_TASK) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:360
#8  timer_task (arg=<optimized out>) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:386


```

This is enough information to investigate the example.

However, in other cases the following input may be relevant, too:

Next there is information about the state and current adress of all threads. This can be an important information if you investigate why you SW "freezes". However, in this example you can see that it is normal that some Threads of the TRGB-Bikecomputer wait in user code (ID5 and ID7), so be careful with conclusions.

```
======================== THREADS INFO =========================
  Id   Target Id          Frame 
* 1    process 1070542680 lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
  2    process 1070515272 0x400559e0 in ?? ()
  3    process 1070551252 0x4216c746 in esp_pm_impl_waiti () at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_pm/pm_impl.c:832
  4    process 1070549852 0x4216c746 in esp_pm_impl_waiti () at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_pm/pm_impl.c:832
  5    process 1070338036 UIFacade::updateHandler (this=0x3fca1750 <ui>) at src/UIFacade.cpp:114
  6    process 1070522212 0x40382a0e in vPortEnterCritical (mux=0x3fced2bc) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  7    process 1070337676 BCLogger::flushAllFiles (this=0x3fcaea94 <bclog>) at src/BCLogger.cpp:115
  8    process 1070535080 0x40382b6e in vPortEnterCritical (mux=0x3fcf0d80) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  9    process 1070523704 0x40382a0e in vPortEnterCritical (mux=0x3fcee2bc) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  10   process 1070524064 0x40382a0e in vPortEnterCritical (mux=0x3fcee1b0) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  11   process 1070423616 0x40382a0e in vPortEnterCritical (mux=0x3fcd1d98) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  12   process 1070368888 0x40382b70 in vPortEnterCritical (mux=0x3fcc77d4) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  13   process 1070387128 0x40382b70 in vPortEnterCritical (mux=0x3fccc4c8) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  14   process 1070395268 0x40382b70 in vPortEnterCritical (mux=0x3fccdc94) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  15   process 1070382024 0x40382b70 in vPortEnterCritical (mux=0x3fccacd8) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  16   process 1070557508 0x40382a0e in vPortEnterCritical (mux=0x3fcf62ec) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  17   process 1070536780 0x40382b70 in vPortEnterCritical (mux=0x3fcf1424) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578

```

Next there is a long list with backtrace of all Threads. You can have a quick look to get an overview if something may be weired, but if you really need to investigate other threads, use `debug_corefile` instead.

```
==================== THREAD 1 (TCB: 0x3fcf2f58, name: 'esp_timer') =====================
#0  lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
#1  0x4200dd56 in ui_ScrChartSetPostFirst (pos=1, idx=<optimized out>) at src/ui/Screens/Chart/ui_Chart_CustFunc.c:19
#2  0x4200b5c8 in UIFacade::setChartPosFirst (this=0x3fca1750 <ui>, pos=1, idx=149 '\\225') at src/UIFacade.cpp:308
#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411
#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112
#5  0x4200a7f0 in Statistics::<lambda(Statistics*)>::operator() (__closure=0x0, thisInstance=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:50
#6  Statistics::<lambda(Statistics*)>::_FUN(Statistics *) () at src/Stats/Statistics.cpp:50
#7  0x42086a11 in timer_process_alarm (dispatch_method=ESP_TIMER_TASK) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:360
#8  timer_task (arg=<optimized out>) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:386

==================== THREAD 2 (TCB: 0x3fcec448, name: 'loopTask') =====================

[...]

```

So, what you can see in the coredump? --> The lvgl library crashed due to a _StoreProhibitedCause_ (attempt to write at forbidden address). 

However, it is important to understand that `ser->start_point = id;` cannot be the real cause. At a more detailed look, you may notice that `lv_chart_series_t* ser` has the value 0x74, which seems like a strange value for a pointer, so look where it comes from:
`lv_chart_set_x_start_point(ui_Chart1, ui_Chart1_series[idx], pos);`

So, it is the value of `ui_Chart1_series[idx]` - the array stores pointers to lv_chart_series_t, but is only of size 4. However, the coredump shows that idx is "optimized out". So have a look one level higher - there idx is '149' - so completely out of bounds. Reading out of bounds is usually possible in C++ because you will access memory just behind the array where some other variables are stored and you will get the value of the other variable.

To understand why we read out of bounds, we need to go up one more level:

`#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411`

This function also got idx as parameter and just passes it through, so we need to go up even more:

`#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112`

There the idx is taken from an for-loop counter:

```C++
for (uint_fast8_t j; j < 4 ; j++) {
	createChartArray(j);
}
```

Do you notice the bug?

Yep - `j` is not initialized and thus has an arbitrary value. To fix it, change the for-loop to:

`for (uint_fast8_t j=0; j < 4 ; j++) {`
















