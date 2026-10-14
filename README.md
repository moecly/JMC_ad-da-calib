V1.00    初版, 实现了DIG_8IN_8OUT_NPN模块
V1.01    增加所有模块的基础版本, ModuleComm默认值初始化为0
V1.02    发行版和调试版
V1.04    CMake编译
V1.06    AD DA模块
V1.07    DA模块相同值不修改，避免开关造成波动
V1.09    校准：新增单板手动校准角色(CALIB_ROLE=MANUAL)、DUT对REF的信息查询、上位机连接恢复与卡片式界面；门限与采样节拍改为运行时参数(OP_HOST_PARAMS)、复检可跳过(skip_verify)
V1.10    修复AD方向校准采样：窗内估计量由min/max中点改为中值(MedianWin)，采样间隔默认5→20ms使窗长跨过AD7689的128ms均值滤波记忆；此前单个异常样本按半幅计入读数，实测残差达5681.5码

