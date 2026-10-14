// 任务四：数据统计报表（MySQL）
// 要求：
//   1. 基于订单表生成每日销售报表
//   2. 统计：订单总数、销售总额、平均客单价
//   3. 使用数据库视图存储统计逻辑
//   4. 实现按日期范围查询统计结果

use anyhow::Result;
use chrono::NaiveDate;
use mysql::prelude::*;
use mysql::{params, Pool};

const DB_URL: &str = "mysql://lab3:lab3pass@127.0.0.1:3306/lab3";

#[derive(Debug)]
#[allow(dead_code)]
struct DailyReportRow {
    day: NaiveDate,
    order_count: i64,
    total_amount: f64,
    avg_amount: f64,
}

/// 建表 orders + 视图 v_daily_sales_report。
fn init_schema(pool: &Pool) -> Result<()> {
    let mut conn = pool.get_conn()?;
    conn.query_drop(
        r"CREATE TABLE IF NOT EXISTS orders (
            order_id   BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            customer   VARCHAR(64) NOT NULL,
            amount     DECIMAL(10, 2) NOT NULL,
            created_at DATETIME NOT NULL
        )",
    )?;
    conn.query_drop(
        r"CREATE OR REPLACE VIEW v_daily_sales_report AS
          SELECT DATE(created_at) AS day,
                 COUNT(*)         AS order_count,
                 SUM(amount)      AS total_amount,
                 AVG(amount)      AS avg_amount
          FROM orders
          GROUP BY DATE(created_at)",
    )?;
    Ok(())
}

/// 插入一些样例订单数据，便于看到报表效果。
fn seed_orders(pool: &Pool) -> Result<()> {
    let mut conn = pool.get_conn()?;

    // 实验演示用：每次跑都重置订单表，保证报表输出可复现。
    conn.query_drop("TRUNCATE TABLE orders")?;

    let orders: &[(&str, &str, f64)] = &[
        ("2026-05-10 09:15:00", "alice",  199.00),
        ("2026-05-10 14:30:00", "bob",     88.50),
        ("2026-05-10 20:05:00", "alice",  320.00),
        ("2026-05-12 11:00:00", "carol",   50.00),
        ("2026-05-12 18:45:00", "dave",  1200.00),
        ("2026-05-15 08:20:00", "alice",  299.99),
        ("2026-05-15 13:10:00", "eve",    150.00),
        ("2026-05-15 16:55:00", "bob",     75.25),
        ("2026-05-15 21:30:00", "carol",  800.00),
        ("2026-05-18 10:00:00", "dave",   450.00),
        ("2026-05-18 19:20:00", "eve",     99.99),
        ("2026-05-20 09:00:00", "alice", 1500.00),
        ("2026-05-20 12:30:00", "bob",    250.50),
        ("2026-05-20 17:45:00", "carol",   30.00),
    ];

    conn.exec_batch(
        "INSERT INTO orders (customer, amount, created_at)
         VALUES (:customer, :amount, :created_at)",
        orders.iter().map(|(ts, customer, amount)| {
            params! {
                "customer"   => customer,
                "amount"     => amount,
                "created_at" => ts,
            }
        }),
    )?;
    Ok(())
}

/// 按日期范围 [start, end] 查询每日报表。
fn query_report(pool: &Pool, start: NaiveDate, end: NaiveDate) -> Result<Vec<DailyReportRow>> {
    let mut conn = pool.get_conn()?;
    let rows = conn.exec_map(
        r"SELECT DATE_FORMAT(day, '%Y-%m-%d') AS day_str,
                 order_count,
                 CAST(total_amount AS CHAR) AS total_str,
                 CAST(avg_amount   AS CHAR) AS avg_str
          FROM v_daily_sales_report
          WHERE day BETWEEN :start AND :end
          ORDER BY day",
        params! {
            "start" => start.format("%Y-%m-%d").to_string(),
            "end"   => end.format("%Y-%m-%d").to_string(),
        },
        |(day_str, order_count, total_str, avg_str): (String, i64, String, String)| {
            let day = NaiveDate::parse_from_str(&day_str, "%Y-%m-%d").unwrap();
            let total_amount = total_str.parse::<f64>().unwrap();
            let avg_amount = avg_str.parse::<f64>().unwrap();
            DailyReportRow { day, order_count, total_amount, avg_amount }
        },
    )?;
    Ok(rows)
}

fn main() -> Result<()> {
    let pool = Pool::new(DB_URL)?;
    init_schema(&pool)?;
    seed_orders(&pool)?;

    let start = NaiveDate::from_ymd_opt(2026, 5, 1).unwrap();
    let end = NaiveDate::from_ymd_opt(2026, 5, 31).unwrap();
    let rows = query_report(&pool, start, end)?;

    println!("销售报表 {start} ~ {end}（共 {} 天）：", rows.len());
    for r in rows {
        println!(
            "  {} | 订单数={} 销售额={:.2} 客单价={:.2}",
            r.day, r.order_count, r.total_amount, r.avg_amount
        );
    }
    Ok(())
}
