sct timer is used in pio_write_deco2d and ICON model with CDI-PIO to measure performance.

# Use sct in pio_write_deco2d
sct timers are enabled for both GRIB2 and NetCDF4 format, on I/O clients and I/O servers now.

sct context is newly used to separate measurements on I/O clients and I/O severs.

> * SCT_PROC_CHOICE=SCT_REDUCE_ALL aggregate the sct timer report, similar to profiling_output=1
> * SCT_PROC_CHOICE=SCT_SELECT_ALL individual sct timer report is printout on each MPI proc, similar to profiling_output=2

We run `pio_write_deco2d` with GRIB2 format, as monsoon uses GRIB2 as well.

File [LOG.test_pio.booster_nvhpc_ompi.srun.5411160.o.txt](LOG.test_pio.booster_nvhpc_ompi.srun.5411160.o.txt) is a log file of running `test_pio.booster_nvhpc_ompi.srun.sh` on Juwels Booster, with SCT_PROC_CHOICE=SCT_SELECT_ALL.

File [LOG.test_pio.booster_nvhpc_ompi.srun.5411226.o.txt](LOG.test_pio.booster_nvhpc_ompi.srun.5411226.o.txt) is a log file of running `test_pio.booster_nvhpc_ompi.srun.sh` on Juwels Booster, with SCT_PROC_CHOICE=SCT_REDUCE_ALL.


# Use sct in ICON model with CDI-PIO
We can get sct report now.

Both ICON model and CDI-PIO are linked with sct libraries, but the sct libraries are in different paths.
CDI-PIO uses sct that CDI-PIO build script compiled. The sct ICON uses is bundled library in ICON.(so we use **distinct** in the log filename)

Regarding monsoon R2B4, we run 4 I/O servers and 8 I/O clients.

File [LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling1.run.5412273.o.txt](LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling1.run.5412273.o.txt) is a log file of running monsoon R2B4 on Juwels Booster, with SCT_PROC_CHOICE=SCT_RECUDE_ALL.
It is still **not perfect**, some sct timers not running on I/O servers are also aggregated(showing 0 second in the min column, see below). 
Maybe we should try to link ICON and CDI-PIO with the same sct library that CDI-PIO build script compiled.
sct timers running on I/O servers show good results.

File [LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling2.run.5412319.o.txt](LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling2.run.5412319.o.txt) is a log file of running monsoon R2B4 on Juwels Booster, with SCT_PROC_CHOICE=SCT_SELECT_ALL.
The result is messy, sct report from I/O servers and clients are mixed. For now, we can only grep to get the information we want.


# sct report result

## pio_write_deco2d

sct report for pio_write_deco2d from [LOG.test_pio.booster_nvhpc_ompi.srun.5411226.o.txt](LOG.test_pio.booster_nvhpc_ompi.srun.5411226.o.txt):

```
 --> sct_report for all timers:
program_mode  = MPI
program start = 2022-04-20 22:02:48
proc_choice   = SCT_REDUCE_ALL
nprocs        = 96
context       = PIO-WRITE TIMER
statistics    = process-statistics

| name         |       min  |     avg  |     max  |     sum  | lbe[%] |
|--------------|------------|----------|----------|----------|--------|
| global total |    65.238  |  65.526  |  65.602  |  6290.5  | 99.885 |


nprocs        = 16
context       = CDI-PIO timer
statistics    = process-statistics
---------------------------------------------------------------------------
 name                   |       min       avg       max       sum   lbe[%]
---------------------------------------------------------------------------
 IO servers total       |    65.092    65.320    65.395    1045.1   99.885
 MPI_Get                |    0.7058    0.8259    0.8985    13.215   91.926
 YAXT exchange1         |    9.8262    12.441    16.575    199.05   75.055
 aggregate buffer flush |    8.5189    11.953    16.186    191.25   73.848
 getTimeStepData        |    56.270    56.362    56.457    901.79   99.831
 post write batch       |    0.0000    0.0000    0.0000    0.0001   66.613
 read getbuffers        |    37.619    41.556    47.313    664.89   87.831
 window complete        |    8.2338    13.931    17.802    222.89   78.255
 window start           |    0.0048    0.0491    0.6044    0.7861    8.130
 write slice            |    6.9872    9.0379    11.902    144.61   75.936
 write stream           |    37.530    37.566    38.102    601.05   98.593
```

## monsoon R2B4

sct report for monsoon R2B4 from [LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling1.run.5412273.o.txt](LOG.exp.luk0003_pio-sct_timer-icon-sct_timer-distinct_profiling1.run.5412273.o.txt):

```
 0:  --> sct_report for all timers:
 0: program_mode  = MPI
 0: program start = 2022-04-21 09:25:32
 0: proc_choice   = SCT_REDUCE_ALL
 0: nprocs        = 12
 0: context       = default context
 0: statistics    = process-statistics
 0: ----------------------------------------------------------------------------------------------
 0:  name                                      |       min       avg       max       sum   lbe[%]
 0: ----------------------------------------------------------------------------------------------
 0:  d2p_couple                                |    0.0000    0.0250    0.0379    0.2995   65.892
 0:  d2p_prep                                  |    0.0000    0.0303    0.0459    0.3635   65.929
 0:  d2p_sync                                  |    0.0000    0.0181    0.0294    0.2170   61.416
 0:  dyn2phy                                   |    0.0000    0.0734    0.1128    0.8802   65.052
 0:  echam_bcs                                 |    0.0000    0.1563    0.2461    1.8759   63.515
 0:  echam_phy                                 |    0.0000    2.6672    4.0452    32.006   65.934
 0:  exch_data                                 |    0.0000    1.7918    2.8657    21.501   62.526
 0:  exch_data.wait                            |    0.0000    0.8454    1.5616    10.144   54.136
 0:  global_sum                                |    0.0000    0.0005    0.0012    0.0058   39.673
 0:  ice_fast                                  |    0.0000    0.0067    0.0102    0.0800   65.482
 0:  iconam_echam                              |    0.0000    3.1300    4.6981    37.559   66.622
 0:  integrate_nh                              |    0.0000    8.0586    12.093    96.703   66.636
 0:  interface_echam_cov                       |    0.0000    0.0151    0.0229    0.1813   66.022
 0:  interface_echam_mig                       |    0.0000    0.0624    0.0961    0.7490   64.938
 0:  interface_echam_rht                       |    0.0000    0.0192    0.0299    0.2308   64.332
 0:  interface_echam_sso                       |    0.0000    0.1785    0.2726    2.1419   65.469
 0:  interface_echam_vdf                       |    0.0000    1.7722    2.6828    21.266   66.059
 0:  interface_echam_wmo                       |    0.0000    0.0115    0.0175    0.1375   65.416
 0:  jsbach                                    |    0.0000    1.2760    1.9347    15.312   65.953
 0:  lrtm                                      |    0.0000    0.2434    0.3694    2.9211   65.906
 0:  nesting                                   |    0.0000    0.0000    0.0000    0.0002   56.640
 0:  nh_diagnostics                            |    0.0000    0.0456    0.0694    0.5478   65.777
 0:  nh_hdiff                                  |    0.0000    0.1551    0.2362    1.8617   65.694
 0:  nh_solve                                  |    0.0000    3.4018    5.1392    40.822   66.194
 0:  ordglb_sum                                |    0.0000    0.0002    0.0003    0.0022   66.393
 0:  p2d_couple                                |    0.0000    0.0400    0.0608    0.4798   65.757
 0:  p2d_prep                                  |    0.0000    0.0736    0.1441    0.8835   51.075
 0:  p2d_sync                                  |    0.0000    0.1190    0.1918    1.4274   62.014
 0:  phy2dyn                                   |    0.0000    0.2326    0.3959    2.7910   58.745
 0:  radiation                                 |    0.0000    0.5772    0.8733    6.9258   66.086
 0:  srtm                                      |    0.0000    0.2848    0.4329    3.4176   65.793
 0:  total                                     |    0.0000    48.284    72.543    579.41   66.560
 0:  transport                                 |    0.0000    1.0182    1.5286    12.219   66.612
 0:  update_surface                            |    0.0000    1.3805    2.0918    16.566   65.995
 0:  vdiff_down                                |    0.0000    0.2452    0.3763    2.9426   65.167
 0:  vdiff_up                                  |    0.0000    0.0352    0.0534    0.4219   65.886
 0:  wrt_output                                |    0.0000    39.786    59.687    477.43   66.657

 8: nprocs        = 4
 8: context       = CDI-PIO timer
 8: statistics    = process-statistics
 8: ----------------------------------------------------------------------------------------------
 8:  name                                      |       min       avg       max       sum   lbe[%]
 8: ----------------------------------------------------------------------------------------------
 8:  IO servers total                          |    73.166    73.166    73.166    292.66  100.000
 8:  MPI_Get                                   |    0.0007    0.0007    0.0007    0.0029   95.803
 8:  YAXT exchange1                            |    0.0675    0.0735    0.0785    0.2941   93.623
 8:  aggregate buffer flush                    |    3.6959    3.7959    3.9038    15.183   97.236
 8:  getTimeStepData                           |    4.5620    4.5926    4.6196    18.370   99.416
 8:  post write batch                          |    0.0000    0.0000    0.0001    0.0002   43.144
 8:  read getbuffers                           |    4.5486    4.5801    4.6076    18.320   99.403
 8:  window complete                           |    0.0092    0.0094    0.0095    0.0375   98.758
 8:  window start                              |    0.0008    0.0016    0.0027    0.0064   58.056
 8:  write slice                               |    0.1715    0.1810    0.1869    0.7240   96.865
 8:  write stream                              |    3.9593    4.0624    4.1695    16.249   97.430
```

## Explain names of sct timer

`IO servers total` tracks when I/O servers is active;

`YAXT exchange1` tracks function xt_redist_s_exchange1();

`aggregate buffer flush` tracks function aggBufFlush();

`getTimeStepData` tracks function getTimeStepData();

`post write batch` tracks function conf->callbacks\[CDIPIO_CALLBACK_POSTWRITEBATCH\]();

`read get buffers` tracks function readGetBuffers();

`window start` tracks function MPI_Win_start();

`window complete` tracks function MPI_Win_complete();

`write slice` tracks function streamWriteVarSlice() or streamWriteVarSliceF() for output in GRIB;

`write stream` tracks NetCDF or GRIB write duration, function writeNetCDFStream() or writeGribStream();

`write chunk` tracks function streamWriteVarChunk() or streamWriteVarChunkF() for output in NetCDF;

