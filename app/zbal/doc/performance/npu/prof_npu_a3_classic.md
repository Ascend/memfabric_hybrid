## Classic communication performance

### Test environment

All testcase running on A3 super pod.

### 1 All2Allv

|Die Num|Input Data Size|Baseline(us)|ZBAL(us)|
|---|---|---|--|
| 2|~128M | 925 | 702 |
| 4|~128M | 999 | 716 |
| 8|~128M | 1527 | 820 |
| 16|~128M | 1610 | 1015 |
| 32|~128M | 4074 | 3432 |
| 64|~128M | 6752 | 5197 |