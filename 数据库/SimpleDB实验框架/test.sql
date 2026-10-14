-- ==========================================
-- SimpleDB 验收测试集 (Test Suite)
-- 建议按顺序执行，观察优化前后的性能差异
-- 可以自己编写测试用的 SQL，能体现结果即可
-- ==========================================

-- ------------------------------------------
-- [Phase 1] 基础算子测试
-- ------------------------------------------

-- 1.1 基础查询
SELECT name, age FROM student WHERE age > 20;

-- 1.2 笛卡尔积测试
SELECT s.name, sc.grade FROM student AS s, score AS sc LIMIT 5;


-- ------------------------------------------
-- [Phase 2] 索引构建
-- ------------------------------------------

-- 2.1 创建索引
.create_index student id
.create_index score student_id

-- ------------------------------------------
-- [Phase 3] 优化器与外围能力测试
-- ------------------------------------------

-- 3.1 选择下推验证
.opt off
SELECT s.name, sc.grade FROM student AS s, score AS sc WHERE sc.student_id = s.id AND sc.student_id = 10;

.opt on
SELECT s.name, sc.grade FROM student AS s, score AS sc WHERE sc.student_id = s.id AND sc.student_id = 10;

-- 对比前后的运行时间性能差异，并通过查看算子树分析优化原理

-- 3.2 内外表顺序优化验证
.opt off
SELECT s.name, sc.grade FROM score AS sc, student AS s WHERE sc.student_id = s.id;

.opt on
SELECT s.name, sc.grade FROM score AS sc, student AS s WHERE sc.student_id = s.id;

-- 对比前后的运行时间性能差异，并通过查看算子树分析优化原理

-- 3.3 索引优化选择验证
.create_index score student_id

.opt off
SELECT * FROM score WHERE student_id = 10;

.opt on
SELECT * FROM score WHERE student_id = 10;

-- 对比前后的运行时间性能差异，并通过查看算子树分析优化原理

-- 3.4 索引优化连接验证

.create_index student id
.opt off
SELECT * FROM student, score WHERE score.student_id = student.id;

.opt on
SELECT * FROM student, score WHERE score.student_id = student.id;

-- 对比前后的运行时间性能差异，并通过查看算子树分析优化原理


-- 3.5 综合压力测试 (3表连接)
.create_index student id
.opt on
SELECT s.name, c.title, sc.grade
FROM student AS s, score AS sc, course AS c
WHERE s.id = sc.student_id
  AND sc.course_id = c.id
  AND c.credits > 3;

-- 时间充裕的情况下可以关闭优化器，体验无优化时的性能
