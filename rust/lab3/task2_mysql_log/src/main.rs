// 任务二：日志记录系统（MySQL）
// 要求：
//   1. 连接本地 MySQL，创建 app_logs 表
//   2. 字段：log_id(自增)、log_level(ENUM)、content、created_at
//   3. 实现日志写入函数，支持插入不同级别的日志
//   4. 实现查询最近 24 小时的 ERROR 级别日志

use anyhow::Result;
use chrono::{DateTime, Duration, Local, NaiveDateTime, TimeZone};
use mysql::prelude::*;
use mysql::{params, Pool};

const DB_URL: &str = "mysql://lab3:lab3pass@127.0.0.1:3306/lab3";

#[derive(Debug, Clone, Copy)]
enum LogLevel {
    Info,
    Warn,
    Error,
    Debug,
}

impl LogLevel {
    fn as_str(&self) -> &'static str {
        match self {
            LogLevel::Info => "INFO",
            LogLevel::Warn => "WARN",
            LogLevel::Error => "ERROR",
            LogLevel::Debug => "DEBUG",
        }
    }
}

#[derive(Debug)]
#[allow(dead_code)]
struct LogRow {
    log_id: u64,
    log_level: String,
    content: String,
    created_at: DateTime<Local>,
}

/// 建表
fn init_schema(pool: &Pool) -> Result<()> {
    let mut conn = pool.get_conn()?;
    conn.query_drop(
        r"CREATE TABLE IF NOT EXISTS app_logs (
            log_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            log_level ENUM('INFO','WARN','ERROR','DEBUG') NOT NULL,
            content TEXT NOT NULL,
            created_at DATETIME NOT NULL
        )",
    )?;
    Ok(())
}

/// 以指定时间戳写入一条日志。
fn write_log_at(pool: &Pool, level: LogLevel, content: &str, when: DateTime<Local>) -> Result<()> {
    let mut conn = pool.get_conn()?;
    conn.exec_drop(
        "INSERT INTO app_logs (log_level, content, created_at) VALUES (:level, :content, :ts)",
        params! {
            "level" => level.as_str(),
            "content" => content,
            "ts" => when.format("%Y-%m-%d %H:%M:%S").to_string(),
        },
    )?;
    Ok(())
}

/// 以当前时间写入一条日志。
#[allow(dead_code)]
fn write_log(pool: &Pool, level: LogLevel, content: &str) -> Result<()> {
    write_log_at(pool, level, content, Local::now())
}

/// 查询最近 24 小时内 ERROR 级别日志。
fn query_recent_errors(pool: &Pool) -> Result<Vec<LogRow>> {
    let mut conn = pool.get_conn()?;
    let rows = conn.query_map(
        r"SELECT log_id, log_level, content,
                 DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s')
          FROM app_logs
          WHERE log_level = 'ERROR'
            AND created_at >= NOW() - INTERVAL 24 HOUR
          ORDER BY created_at DESC",
        |(log_id, log_level, content, ts_str): (u64, String, String, String)| {
            let naive = NaiveDateTime::parse_from_str(&ts_str, "%Y-%m-%d %H:%M:%S").unwrap();
            let created_at = Local.from_local_datetime(&naive).unwrap();
            LogRow { log_id, log_level, content, created_at }
        },
    )?;
    Ok(rows)
}

fn main() -> Result<()> {
    let pool = Pool::new(DB_URL)?;
    init_schema(&pool)?;

    pool.get_conn()?.query_drop("TRUNCATE TABLE app_logs")?;

    let now = Local::now();
    // (相对当前的偏移小时, 等级, 内容)
    let samples: &[(i64, LogLevel, &str)] = &[
        // 24 小时窗口之外
        (-72, LogLevel::Info,  "三天前：服务启动"),
        (-50, LogLevel::Error, "两天前：磁盘 IO 错误"),
        (-30, LogLevel::Warn,  "30 小时前：内存占用偏高"),
        (-26, LogLevel::Error, "26 小时前：缓存命中率骤降"),
        (-25, LogLevel::Error, "25 小时前：上游 API 超时"),
        // 24 小时窗口之内
        (-23, LogLevel::Info,  "23 小时前：服务重启完成"),
        (-20, LogLevel::Warn,  "20 小时前：磁盘占用 80%"),
        (-18, LogLevel::Error, "18 小时前：数据库连接失败"),
        (-12, LogLevel::Debug, "12 小时前：调试信息 x=42"),
        (-8,  LogLevel::Error, "8 小时前：下游服务超时"),
        (-5,  LogLevel::Warn,  "5 小时前：CPU 使用率 90%"),
        (-3,  LogLevel::Error, "3 小时前：支付回调签名校验失败"),
        (-1,  LogLevel::Info,  "1 小时前：每小时定时任务完成"),
        (0,   LogLevel::Error, "刚刚：消息队列消费者掉线"),
    ];

    for (delta_h, level, msg) in samples {
        write_log_at(&pool, *level, msg, now + Duration::hours(*delta_h))?;
    }

    let errs = query_recent_errors(&pool)?;
    println!("最近 24 小时 ERROR 共 {} 条：", errs.len());
    for row in errs {
        println!("  {:?}", row);
    }
    Ok(())
}
