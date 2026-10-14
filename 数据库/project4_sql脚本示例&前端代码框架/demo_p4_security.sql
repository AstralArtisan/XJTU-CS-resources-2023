/*
 * ==============================================================================
 * Project 4 参考样例：触发器与权限配置
 * ==============================================================================
 * 本脚本包含复杂的 OpenGauss 专用语法，请仔细阅读注释进行修改。
 * ==============================================================================
 */

-- ========================================================
-- 模块 A: 完整性控制 (拦截脏数据)
-- 示例目标：防止录入“高成本烂片” (预算>10000 且 评分<5.0)
-- ========================================================

-- 1. 清理旧对象 (防止重复创建报错)
DROP TRIGGER IF EXISTS trg_check_quality ON movies;
DROP FUNCTION IF EXISTS check_movie_quality();

-- 2. 定义触发器函数 (业务逻辑写在这里)
CREATE OR REPLACE FUNCTION check_movie_quality() RETURNS TRIGGER AS $$
BEGIN
    -- [TODO: 请修改这里的 IF 判断逻辑]
    -- NEW 代表即将插入的新数据行
    -- 示例逻辑：如果 budget > 10000 且 director_score < 5.0，则报错
    IF NEW.budget > 10000 AND NEW.director_score < 5.0 THEN
        RAISE EXCEPTION '数据完整性拦截：禁止录入高成本(>1w)低评分(<5.0)的烂片！';
    END IF;
    
    RETURN NEW; -- 必须返回 NEW，否则数据插不进去
END;
$$ LANGUAGE plpgsql;

-- 3. 绑定触发器 (格式通常固定，只需改表名)
-- [TODO: 改为你的表名]
CREATE TRIGGER trg_check_quality
BEFORE INSERT ON movies  
FOR EACH ROW EXECUTE PROCEDURE check_movie_quality();


-- ========================================================
-- 模块 B: 安全性控制 (用户权限与视图)
-- 示例目标：创建分析师账号，只让他看视图，不让他看预算(budget)
-- ========================================================

-- 1. 创建受限用户
DO $$
BEGIN
    -- 检查用户是否存在，如果存在则收回 Schema 权限
    IF EXISTS (SELECT 1 FROM pg_catalog.pg_roles WHERE rolname = 'analyst_user') THEN
        REVOKE USAGE ON SCHEMA gaussdb FROM analyst_user;
        REVOKE ALL ON ALL TABLES IN SCHEMA gaussdb FROM analyst_user;
    END IF;
END $$;
DROP USER IF EXISTS analyst_user;
CREATE USER analyst_user WITH PASSWORD 'Analyst@123';

-- 2. 创建脱敏视图 (故意不包含 budget 敏感字段)
-- [TODO: 改为你的表名，并只选择非敏感字段]
CREATE OR REPLACE VIEW v_public_movies AS
SELECT id, title, director_score FROM movies; 

-- 3. 权限分配
GRANT SELECT ON v_public_movies TO analyst_user;
REVOKE ALL ON movies FROM analyst_user; -- 确保不能查原表

-- ========================================================
-- 模块 C: 环境补丁 (DO NOT CHANGE / 请勿修改)
-- 解决 OpenGauss "relation does not exist" 报错的关键代码
-- ========================================================
GRANT USAGE ON SCHEMA gaussdb TO analyst_user;
ALTER USER analyst_user SET search_path TO "$user", gaussdb, public;

-- [验证] 尝试查询视图
SELECT * FROM v_public_movies LIMIT 1;
