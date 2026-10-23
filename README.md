V1.00    初版, 实现了DIG_8IN_8OUT_NPN模块
V1.01    增加所有模块的基础版本, ModuleComm默认值初始化为0
V1.02    发行版和调试版
V1.04    CMake编译
V1.06    AD DA模块
V1.07    DA模块相同值不修改，避免开关造成波动
V1.09    校准：新增单板手动校准角色(CALIB_ROLE=MANUAL)、DUT对REF的信息查询、上位机连接恢复与卡片式界面；门限与采样节拍改为运行时参数(OP_HOST_PARAMS)、复检可跳过(skip_verify)
V1.10    修复AD方向校准采样：窗内估计量由min/max中点改为中值(MedianWin)，采样间隔默认5→20ms使窗长跨过AD7689的128ms均值滤波记忆；此前单个异常样本按半幅计入读数，实测残差达5681.5码
V1.11    justfile 构建脚本改为板型×角色×类型三维矩阵（REF/DUT/MANUAL，去掉APP），并发配置与构建，目录 build/<BOARD>_<ROLE>_<TYPE>
V1.12    AD/DA 标定：拟合残差/复检超差改为「只重测超标点」重试（retry_count 默认 3，含首测；重测值仅在更贴合拟合时才采纳），拟合残差门限默认 100→20；上位机(host/prod)图表标出重测点并可叠加复检曲线，修复图表刷新时视图跳顶与表格折行
V1.13    手动校准修复与完善：修复 role MANUAL 误置会话导致「开始」不可用；DA 同 raw、AD 同 ideal 重采覆盖原点（满 16 点仍可改）；手动拟合加残差门限与无点/无 raw 保护；host 新增手动测量曲线、采集中/覆盖提示，修正 DA 读数行 ideal/raw 列错位
V1.14    手动校准采集点数取消固定上限：固件按需动态扩容（受堆内存限制，不再受 16 点硬限制），拟合改用 PolyFitU16 避免大点数占用栈；host 移除满点禁用与满点提示
V1.15    手动校准支持导入/删除采样点：host 可粘贴或从文件导入点表（每行 ideal raw 或 n ideal raw，同键覆盖），采样点表逐点删除（按 ideal+raw 定位，删除后重新编号）；固件新增 OP_HOST_MANUAL_IMPORT/REMOVE，下行限速 24 B/轮避免 RTT 环形缓冲溢出
