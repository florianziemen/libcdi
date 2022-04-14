File *LOG.test_pio.booster_nvhpc_ompi.srun.5394895.o.txt* is a log file of running batch file `test_pio.booster_nvhpc_ompi.srun.sh` on Juwels Booster. 
This batch file above executes `pio_write_deco2d` with several sct timers inserted. Here we only focus on the sct report from I/O server No. 8.

| name                |     proc | #calls   |     min  |     avg   |    max   |    sum |
|--------------------:|---------:|---------:|---------:|----------:|---------:|-------:|
| total               |        8 |      1   |  139.91  |  139.91   | 139.91   | 139.91 |
| getTimeStepData     |        8 |     20   |  5.6910  |  6.4396   | 7.2031   | 128.79 |
| window start        |        8 |     20   |  0.0000  |  0.0004   | 0.0043   | 0.0083 |
| MPI_Get             |        8 |    100   |  0.0000  |  0.0091   | 0.1144   | 0.9148 |
| window complete     |        8 |     20   |  0.4489  |  0.7451   | 0.7958   | 14.902 |
| read getbuffers     |        8 |     20   |  4.9236  |  5.6484   | 6.4396   | 112.97 |
| write NetCDF        |        8 |     20   |  4.9227  |  5.6436   | 6.3912   | 112.87 |
| prepare write chunk |        8 |    240   |  0.0000  |  0.0000   | 0.0001   | 0.0067 |
| create write deco   |        8 |    240   |  0.0000  |  0.0001   | 0.0062   | 0.0237 |
| YAXT exchange1      |        8 |   2400   |  0.0001  |  0.0033   | 0.0186   | 7.8704 |
| write chunk         |        8 |   2400   |  0.0009  |  0.0437   | 0.4685   | 104.96 |
| post write batch    |        8 |     20   |  0.0000  |  0.0000   | 0.0000   | 0.0000 |

 `getTimeStepData()` costed the most time while I/O servers were active -- 92%

 `streamWriteVarChunk()`(write chunk) costed the most time while `getTimeStepData()` were called -- 81%

 `xt_redist_s_exchange1()`(YAXT exchange1) costed almost 8 seconds, 6% while `getTimeStepData()` were called

 **Remarkable:** `MPI_Win_complete` costed alomost 15 seconds, 12% while `getTimeStepData()` were called.
