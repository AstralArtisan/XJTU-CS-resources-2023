import threading
import time

import psycopg2
import streamlit as st

# ==============================================================================
# 🎯 模块 1：实验场景与系统参数配置(修改你的自定义字段)
# ==============================================================================
SCENARIO_CONFIG = {
    "TABLE_NAME": "tickets",  # 目标关系表名称
    "ID_COL": "id",  # 主键属性名
    "STOCK_COL": "count",  # 竞争资源属性名（库存）
    "TARGET_ID": 1,  # 竞争目标的元组 ID
    "APP_TITLE": "数据库并发实验：票务系统",
    "ITEM_NAME": "电影票",
    "UNIT": "张",
    "ACTION_NAME": "抢票",
}

# ==============================================================================
# 🔧 模块 2：数据库连接与状态管理接口
# ==============================================================================
DB_CONFIG = {
    "dbname": "postgres",
    "user": "gaussdb",
    "password": "YourPassword@123",  # 修改密码
    "host": "localhost",
    "port": "5432",
}

TBL = SCENARIO_CONFIG["TABLE_NAME"]
ID = SCENARIO_CONFIG["ID_COL"]
STOCK = SCENARIO_CONFIG["STOCK_COL"]
T_ID = SCENARIO_CONFIG["TARGET_ID"]


def get_conn():
    """获取数据库连接对象"""
    return psycopg2.connect(**DB_CONFIG)


def init_inventory(initial_count):
    """
    初始化实验环境状态
    作用：将目标资源的库存重置为指定值，确保实验的可重复性。
    """
    conn = None
    try:
        conn = get_conn()
        cur = conn.cursor()
        # 执行更新操作，重置库存
        sql = f"UPDATE {TBL} SET {STOCK} = %s WHERE {ID} = %s;"
        cur.execute(sql, (initial_count, T_ID))
        conn.commit()
        return True, f"✅ 环境重置成功：{SCENARIO_CONFIG['ITEM_NAME']}库存已设为 {initial_count}"
    except Exception as e:
        return False, f"❌ 初始化失败: {e}"
    finally:
        if conn:
            conn.close()


def get_real_stock():
    """
    获取当前数据库的一致性状态
    作用：实时查询数据库中的实际库存值，作为真值 (Ground Truth) 用于验证。
    """
    conn = None
    try:
        conn = get_conn()
        cur = conn.cursor()
        sql = f"SELECT {STOCK} FROM {TBL} WHERE {ID} = %s;"
        cur.execute(sql, (T_ID,))
        res = cur.fetchone()
        return res[0] if res else 0
    except psycopg2.Error as e:
        # 捕获数据库层面的连接或语法异常
        print(f"[DB Error] get_real_stock failed: {e}")
        return -1
    finally:
        if conn:
            conn.close()


# ==============================================================================
# 📝 模块 3：并发事务处理逻辑 (核心实验代码)
# ==============================================================================
def concurrency_worker(thread_name, delay_time, use_lock, log_list):
    """
    模拟单个用户的并发事务行为
    参数:
        delay_time: 模拟业务处理耗时，用于扩大竞态条件的时间窗口。
        use_lock: 是否应用排他锁机制。
    """
    conn = None
    try:
        conn = get_conn()
        cur = conn.cursor()

        # ------------------------------------------------------------------
        # 👇👇👇 [TODO: 任务 1] 定义事务隔离策略与锁机制 👇👇👇
        # 说明：根据实验要求，选择“普通快照读取”或“当前读并加锁”。
        # ------------------------------------------------------------------
        sql = ""
        if use_lock:
            # [悲观锁策略]
            # 使用 FOR UPDATE 语句对目标元组加排他锁 (X锁)，实现事务的串行化调度。
            sql = f"SELECT {STOCK} FROM {TBL} WHERE {ID} = {T_ID} FOR UPDATE;"
        else:
            # [默认策略]
            # 使用普通 SELECT 语句，在 Read Committed 级别下读取快照数据。
            # 此操作不会阻塞其他事务，但可能读取到稍后失效的数据。
            sql = f"SELECT {STOCK} FROM {TBL} WHERE {ID} = {T_ID};"

        cur.execute(sql)
        # ------------------------------------------------------------------

        res = cur.fetchone()
        if not res:
            log_list.append(f"[{thread_name}] ❌ 错误: 目标对象不存在")
            return

        current_val = res[0]

        # [模拟竞态条件]
        # 强制线程休眠，模拟“读-改-写”过程中的业务处理耗时。
        # 这是复现“丢失修改”异常的关键步骤。
        time.sleep(delay_time)

        # ------------------------------------------------------------------
        # 👇👇👇 [TODO: 任务 2] 执行写操作与事务提交 👇👇👇
        # 说明：基于读取到的值计算新库存，并写入数据库。
        # ------------------------------------------------------------------
        if current_val > 0:
            # 执行原子更新操作
            update_sql = f"UPDATE {TBL} SET {STOCK} = %s WHERE {ID} = %s;"
            cur.execute(update_sql, (current_val - 1, T_ID))

            # 提交事务 (Commit)，持久化修改
            conn.commit()

            action = SCENARIO_CONFIG["ACTION_NAME"]
            log_list.append(f"[{thread_name}] 🎉 {action}成功! (剩余:{current_val - 1})")
        else:
            # 库存不足，回滚事务 (Rollback)
            conn.rollback()
            log_list.append(f"[{thread_name}] 😭 失败 (库存不足)")
        # ------------------------------------------------------------------

    except Exception as e:
        if conn:
            conn.rollback()
        log_list.append(f"[{thread_name}] 💥 异常: {e}")
    finally:
        if conn:
            conn.close()


# ==============================================================================
# 🖥️ 模块 4：实验控制台与可视化监控
# ==============================================================================
st.set_page_config(page_title="P3: 并发实验", layout="wide")
st.title(f"🛡️ {SCENARIO_CONFIG['APP_TITLE']}")

# --- 侧边栏：实验参数设置 ---
st.sidebar.header("🎛️ 实验参数配置")
item_name = SCENARIO_CONFIG["ITEM_NAME"]
unit = SCENARIO_CONFIG["UNIT"]

init_stock = st.sidebar.number_input(f"初始{item_name}数量 ({unit})", min_value=1, value=5)
thread_cnt = st.sidebar.slider("并发线程数 (模拟用户量)", 2, 50, 10)
sim_delay = st.sidebar.slider("业务处理延迟 (秒)", 0.0, 0.5, 0.1)

st.sidebar.divider()
st.sidebar.markdown(f"**当前场景配置：** 表 `{TBL}` | 列 `{STOCK}`")

# --- 主界面：状态监控 ---
# 使用 empty() 创建动态占位符，用于实时展示数据库的一致性状态
c1, c2, c3 = st.columns(3)
stock_placeholder = c1.empty()
c2.metric("理论预期售出", f"{min(init_stock, thread_cnt)} {unit}")
c3.metric("模拟延迟设定", f"{sim_delay} s")

# 初始化显示当前数据库状态
real_stock = get_real_stock()
stock_placeholder.metric("数据库实际库存", f"{real_stock} {unit}")

st.divider()


# --- 实验执行控制逻辑 ---
def run_experiment(use_lock):
    """
    编排并发实验流程
    过程：重置环境 -> 启动多线程 -> 实时监控 -> 验证最终一致性。
    """
    status, msg = init_inventory(init_stock)

    # 重置后立即刷新界面显示
    stock_placeholder.metric("数据库实际库存", f"{init_stock} {unit}")

    if not status:
        st.error(msg)
        return

    logs = []
    threads = []

    # 初始化并启动并发线程
    for i in range(thread_cnt):
        t = threading.Thread(target=concurrency_worker, args=(f"User-{i + 1}", sim_delay, use_lock, logs))
        threads.append(t)
        t.start()

    # [实时监控]
    # 在主线程中轮询数据库状态，动态刷新前端界面，直至所有子线程结束。
    with st.spinner(f"正在模拟 {thread_cnt} 个并发事务..."):
        while any(t.is_alive() for t in threads):
            current_stock = get_real_stock()
            stock_placeholder.metric("数据库实际库存", f"{current_stock} {unit}")
            time.sleep(0.1)  # 采样间隔 0.1秒

    # 等待所有线程执行完毕 (Join)
    for t in threads:
        t.join()

    # 实验结束，获取最终一致性状态
    final_stock = get_real_stock()
    stock_placeholder.metric("数据库实际库存", f"{final_stock} {unit}")

    # 展示执行日志
    st.text_area("事务执行日志", "\n".join(logs), height=300)

    # [结果验证]
    # 对比“理论预期值”与“实际数据库值”，判断是否发生并发异常。
    sold = init_stock - final_stock
    expected_sold = min(init_stock, thread_cnt)

    if not use_lock:
        if sold < expected_sold:
            st.error(
                f"❌ 观测到【丢失修改】异常！\n理论应售出 {expected_sold}，实际数据库仅减少 {sold}。\n说明发生了写-写冲突，部分事务的更新被覆盖。"
            )
        else:
            st.warning(
                "⚠️ 未观测到数据不一致。\n可能原因：系统调度巧合导致串行执行。建议增加【业务处理延迟】参数后重试。"
            )
    else:
        if sold == expected_sold:
            st.success(
                f"✅ 数据一致性验证通过！\n成功售出 {sold} {unit}，库存准确。\n排他锁成功实现了事务的串行化调度。"
            )
            st.balloons()
        else:
            st.error("❌ 数据依然不一致？请检查代码逻辑，确保事务已正确提交 (COMMIT)。")


col_left, col_right = st.columns(2)

with col_left:
    st.subheader("场景 A: 无锁并发测试")
    st.caption("验证 Read Committed 隔离级别下的丢失修改现象")
    if st.button("🔥 启动并发测试 (无锁)", use_container_width=True):
        run_experiment(use_lock=False)

with col_right:
    st.subheader("场景 B: 悲观锁并发测试")
    st.caption("验证排他锁 (For Update) 对并发调度的控制效果")
    if st.button("🔒 启动并发测试 (加锁)", type="primary", use_container_width=True):
        run_experiment(use_lock=True)
