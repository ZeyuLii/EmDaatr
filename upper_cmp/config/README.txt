配置格式为：NAME VALUE1 VALUE2 ... VALUEn     数据间以空格隔开
/**************************MAC层配置设定****************************/
SUBNET：子网号
NODEID：节点ID（必须从1开始，且为连续的！！！）
SUBNET_NODE_NUM：当前在网节点数
IMPORTANT_NODE：子网重要节点ID，顺序为：网管节点 网关节点 备份网关节点
NODE_TYPE：节点身份，其中：
    MANAGEMENT：网管节点
    ORDINARY：普通节点
    GATEWAY：网关节点  
    STANDBY_GATEWAY：备份网关节点
MANA_CHANEL_BUILD_STATE_SLOT_NODE_ID：建链阶段低频信道时隙表节点ID（1-20）
MANA_CHANEL_BUILD_STATE_SLOT_STATE：建链阶段低频信道时隙表状态
    0：空闲
    5：飞行状态信息广播
MANA_CHANEL_OTHER_STATE_SLOT_NODE_ID：其他阶段低频信道时隙表节点ID（1-20）
MANA_CHANEL_OTHER_STATE_SLOT_STATE：其他阶段低频信道时隙表状态
    0：空闲
    3：接入请求
    4：接入请求回复
    5：飞行状态信息广播
    6：网管节点信息通告
    7：上层网管信息广播
