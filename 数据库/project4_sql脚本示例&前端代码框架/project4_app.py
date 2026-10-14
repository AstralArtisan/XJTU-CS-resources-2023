import pandas as pd
import psycopg2
import streamlit as st

# ==============================================================================
# 🎯 模块 1：数据库连接配置与角色定义
# ==============================================================================

# 1. 数据库管理员 (DBA)
# 拥有数据库的最高权限 (Superuser)，用于管理模式和执行特权操作。
ADMIN_CONFIG = {
    "dbname": "postgres",
    "user": "gaussdb",
    "password": "YourPassword@123",  # [TODO: 请核对您的管理员密码]
    "host": "localhost",
    "port": "5432",
}

# 2. 普通用户 / 数据分析师 (Analyst)
# 仅拥有特定视图的查询权限，受限于自主存取控制 (DAC) 策略。
# [TODO: 任务 1] 若在 SQL 脚本中自定义了用户名或密码，请在此处同步更新
ANALYST_CONFIG = {
    "dbname": "postgres",
    "user": "analyst_user",
    "password": "Analyst@123",
    "host": "localhost",
    "port": "5432",
}


# ==============================================================================
# 🔧 模块 2：数据库连接工厂
# ==============================================================================
def get_connection(role_name):
    """根据当前会话的角色身份，建立对应的数据库连接实例"""
    try:
        if role_name == "管理员 (Admin)":
            return psycopg2.connect(**ADMIN_CONFIG)
        else:
            return psycopg2.connect(**ANALYST_CONFIG)
    except Exception:
        return None


# ==============================================================================
# 🖥️ 模块 3：安全攻防实验控制台
# ==============================================================================
st.set_page_config(page_title="Project 4: 安全与完整性", layout="wide")
st.title("🛡️ Project 4: 数据库安全性与完整性控制实验")

# --- 侧边栏：用户身份模拟 ---
st.sidebar.header("身份验证模拟")
# 模拟用户登录过程，切换不同的数据库连接上下文
role = st.sidebar.radio("请选择登录角色:", ["管理员 (Admin)", "分析师 (Analyst)"])

st.info(f"当前会话身份：**{role}**")

# ========================================================
# 模块 A: 自主存取控制 (Security & DAC)
# ========================================================
st.subheader("1. 数据安全性与视图机制测试")

col1, col2 = st.columns([3, 1])
with col1:
    if st.button("执行查询请求"):
        conn = get_connection(role)
        if not conn:
            st.error("连接建立失败，请检查数据库配置")
        else:
            try:
                # ---------------------------------------------------
                # 访问策略：基于角色权限决定访问物理表或视图
                # [TODO: 任务 2] 请将下方的表名/视图名替换为您的选题对象
                # ---------------------------------------------------
                if role == "管理员 (Admin)":
                    # DBA 权限：直接访问物理基表 (Base Table)
                    # 示例：SELECT * FROM movies ...
                    sql = "SELECT * FROM movies ORDER BY id DESC;"
                    st.caption(f"执行 SQL: {sql} (物理基表)")
                else:
                    # 分析师权限：仅允许访问脱敏视图 (Virtual View)
                    # 示例：SELECT * FROM v_public_movies ...
                    sql = "SELECT * FROM v_public_movies ORDER BY id DESC;"
                    st.caption(f"执行 SQL: {sql} (脱敏视图)")

                df = pd.read_sql(sql, conn)
                st.dataframe(df, use_container_width=True)

                # [TODO: 任务 3] 修改此处硬编码的 'budget'，将其替换为您定义的敏感属性名
                SENSITIVE_COL = "budget"

                # 验证敏感属性 (Attribute) 的可见性
                if SENSITIVE_COL in df.columns:
                    st.warning(f"⚠️ 警告：当前权限可见敏感属性 [{SENSITIVE_COL}]")
                else:
                    st.success(f"✅ 安全验证通过：敏感属性 [{SENSITIVE_COL}] 已被视图机制屏蔽")

                conn.close()
            except Exception as e:
                st.error(f"查询异常: {e}")

# 越权访问测试 (Privilege Escalation Test)
if role == "分析师 (Analyst)":
    with col2:
        st.write("")  # 布局占位
        st.write("")
        if st.button("⚠️ 越权访问测试: 强行查询基表"):
            try:
                conn = get_connection(role)
                # 尝试绕过视图，直接对物理表执行 SELECT 操作
                # [TODO: 任务 4] 确保此处查询的是您的物理原表
                pd.read_sql("SELECT * FROM movies", conn)
            except Exception as e:
                # 预期行为：捕获数据库内核抛出的“权限拒绝”异常
                st.error(f"⛔ 访问控制生效！\n数据库内核拒绝了该请求: {e}")

st.divider()

# ========================================================
# 模块 B: 语义完整性约束 (Semantic Integrity)
# ========================================================
st.subheader("2. 业务规则完整性与触发器测试")

if role == "分析师 (Analyst)":
    st.warning("⛔ 权限限制：当前角色不具备数据录入 (INSERT) 权限")
else:
    st.write("测试用例：尝试录入违反业务规则（如：高成本且低评分）的数据，验证触发器拦截机制。")

    with st.form("data_entry_form"):
        # ---------------------------------------------------
        # [TODO: 任务 5] 自定义录入表单
        # 请根据您的表结构，修改下方的输入控件和标签
        # ---------------------------------------------------
        c1, c2, c3 = st.columns(3)

        # 示例：电影名称、预算、评分
        input_1 = c1.text_input("实体名称 (Title)", "违规数据样本")
        input_2 = c2.number_input("敏感数值 (Budget/Cost)", min_value=0, value=50000)
        input_3 = c3.slider("关键指标 (Score/Stock)", 0.0, 10.0, 4.0)

        submitted = st.form_submit_button("执行插入事务 (INSERT)")

        if submitted:
            conn = get_connection(role)
            try:
                cur = conn.cursor()

                # ---------------------------------------------------
                # [TODO: 任务 6] 编写 INSERT 语句
                # 请将 movies 替换为您的表名，并对应上述输入变量
                # ---------------------------------------------------
                insert_sql = "INSERT INTO movies (title, budget, director_score) VALUES (%s, %s, %s)"
                cur.execute(insert_sql, (input_1, input_2, input_3))

                conn.commit()
                st.success("事务提交成功：数据符合完整性约束。")
            except psycopg2.DatabaseError as e:
                conn.rollback()  # 发生异常时回滚事务
                # 捕获并解析数据库抛出的用户定义异常 (User-defined Exception)
                error_msg = str(e).split("CONTEXT:")[0]
                st.error(f"❌ 事务被中断 (触发器拦截):\n{error_msg}")
            finally:
                conn.close()
