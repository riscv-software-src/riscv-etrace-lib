# RISC-V efficient trace (etrace) library

## Description
The RISC-V efficient trace (etrace) library helps with encoding/decoding of RISC-V
etrace packets. This libray can be used as reference for implementing RISC-V etrace
in RISC-V emulators and simulators.

## Build
### For cross compilation:
`export CROSS_COMPILE=riscv64-unkown-linux-gnu-`

### Make
`make clean all`

## Contributing
The Risc-V E-Trace library project encourages and welcomes contributions subject to the following guidelines:
- Sign-off (DCO): All contributions must include a `Signed-off-by:`` line in the commit message to adhere to the Developer Certificate of Origin (DCO).
- Patch Naming: Use clear subjects for emails/pull requests, such as [PATCH] lib: Fix a bug in....
- Machine-Readable Licensing: Use SPDX identifiers (e.g., // SPDX-License-Identifier: BSD-2-Clause) in all new source files to ensure automated compliance

## Usage
### Capturing perf data
For the perf tool to be able to capture E-Trace packets you need:
- Latest upstream Qemu
- Linux kernel with E-Trace driver patches
- Perf tool with support for E-Trace

Since the E-Trace support in Qemu and Linux is currently in development, the relevant patches need to be found on the corresponding development mailing lists.

After booting linux on Qemu `virt` machine, run the command below to capture the perf data:
```
perf record --all-cpus -e rvtrace/event=0x1/ <workload>
```
Currently, `event` only takes the default value `0x1` but it could be improved in future to control the trace behavior. The command above generates the `perf.dat` output file which contains data for the `auxtrace` event.
### Interpreting perf data
The auxtrace data contained in perf.dat needs to be interpreted using the test_etrace_pktdump.elf tool as shown below:
```
./test/test_etrace_pktdump.elf -c <cpu> -p test/itrace_params.csv -d path/to/perf.dat
```
The command above will generate an output as shown below where the auxtrace data will be interpreted as E-Trace packets:
```
0000000000000000:08B: packet00000000: format0 subformat 0 branch_count=0 branch_fmt=0x0
0000000000000008:17B: packet00000001: format0 subformat 0 branch_count=138 branch_fmt=0x0
packet00000002:packet00000009: NULL
0000000000000020:02B: packet00000010: format0 subformat 0 branch_count=0 branch_fmt=0x0
packet00000011:packet00000013: NULL
0000000000000024:32B: packet00000014: format3 subformat3 ienable=1 encoder_mode=0xfffff qual_status=0x0 ioptions=0x0 denable=0 dloss=0
0000000000000044:32B: packet00000015: format3 subformat3 ienable=1 encoder_mode=0x184803ff qual_status=0x1 ioptions=0x130c6c8 denable=1 dloss=1 doptions=0x7fffffff
0000000000000064:25B: packet00000016: format0 subformat 0 branch_count=-16 branch_fmt=0x3 address=0x130b670048500007 notify=0 updiscon=0 irreport=0
000000000000007d:01B: packet00000017: format0 subformat 0 branch_count=0 branch_fmt=0x0
000000000000007e:32B: packet00000018: format3 subformat3 ienable=1 encoder_mode=0x7ffff qual_status=0x0 ioptions=0x39001214 denable=1 dloss=0 doptions=0xffff003a
000000000000009e:25B: packet00000019: format1 branches=31 branch_map=0x7ffffc05 address=0x38e08c2400001fff notify=1 updiscon=0 irreport=1 irdepth=0x7
00000000000000b7:29B: packet00000020: format1 branches=18 branch_map=0x7fffc00e address=0x749dc88c2401ffff notify=0 updiscon=0 irreport=0
00000000000000d4:32B: packet00000021: format3 subformat3 ienable=1 encoder_mode=0x184803ff qual_status=0x1 ioptions=0xa4638 denable=1 dloss=1 doptions=0x7fffffff
00000000000000f4:30B: packet00000022: format0 subformat 0 branch_count=-16 branch_fmt=0x3 address=0xa4638048500007 notify=0 updiscon=0 irreport=0
packet00000023:packet00000025: NULL
0000000000000114:12B: packet00000026: format1 branches=6 branch_map=0x0 address=0xffffffff801d26ea notify=0 updiscon=1 irreport=1 irdepth=0x0
0000000000000120:10B: packet00000027: format3 subformat0 branch=0 privilege=0x1 address=0xffffffff801d26ea
packet00000028:packet00000030: NULL
000000000000012c:11B: packet00000031: format1 branches=2 branch_map=0x0 address=0xffffffff801cd4e8 notify=0 updiscon=1 irreport=1 irdepth=0x0
packet00000032:packet00000033: NULL
0000000000000138:10B: packet00000034: format3 subformat0 branch=0 privilege=0x1 address=0xffffffff801cd4e8
packet00000035:packet00000037: NULL
0000000000000144:10B: packet00000038: format3 subformat0 branch=1 privilege=0x6 address=0x7fffffffe0255d0
packet00000039:packet00000041: NULL
0000000000000150:10B: packet00000042: format3 subformat0 branch=0 privilege=0x1 address=0xffffffff80957434
packet00000043:packet00000045: NULL
000000000000015c:10B: packet00000046: format3 subformat2 privilege=0x0
packet00000047:packet00000049: NULL
0000000000000168:10B: packet00000050: format3 subformat0 branch=0 privilege=0x1 address=0xffffffff800e3882
```

## License
BSD-2-Clause
