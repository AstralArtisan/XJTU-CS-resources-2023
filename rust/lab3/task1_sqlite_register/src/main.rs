// 任务一：用户注册系统（SQLite）
// 要求：
//   1. 创建 SQLite 数据库 users.db
//   2. 建立 users 表 (id 主键、username 唯一、password、created_at 时间戳)
//   3. 实现用户注册功能，插入至少 5 条测试数据
//   4. 实现按用户名查询用户信息

use std::io::{self, BufRead, Write};

use anyhow::Result;
use chrono::{DateTime, Local};
use rusqlite::{Connection, OptionalExtension};

#[derive(Debug)]
#[allow(dead_code)]
struct User {
    id: i64,
    username: String,
    password: String,
    created_at: DateTime<Local>,
}

/// 打开数据库连接，并在不存在时创建 users 表。
fn init_db(path: &str) -> Result<Connection> {
    let conn = Connection::open(path)?;
    conn.execute(
        "CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            created_at DATETIME NOT NULL
        )", 
        [],
    )?;
    Ok(conn)
}

/// 注册一个新用户。username 已存在时应返回错误（依赖 UNIQUE 约束）。
fn register_user(conn: &Connection, username: &str, password: &str) -> Result<i64> {
    let now = Local::now();
    conn.execute(
        "INSERT INTO users (username, password, created_at) VALUES (?1, ?2, ?3)",
        rusqlite::params![username, password, now],
    )?;
    Ok(conn.last_insert_rowid())
}

/// 根据用户名查询用户信息。
fn find_user_by_username(conn: &Connection, username: &str) -> Result<Option<User>> {
    conn.query_row(
        "SELECT id, username, password, created_at FROM users WHERE username = ?1",
        rusqlite::params![username],
        |row| {
            Ok(User {
                id : row.get(0)?,
                username : row.get(1)?,
                password : row.get(2)?,
                created_at : row.get(3)?,
            })
        },
    )
    .optional()
    .map_err(Into::into)
}

/// 从终端读一行，去掉首尾空白。EOF（Ctrl+Z / Ctrl+D）时返回 None。
fn read_line(prompt: &str) -> Result<Option<String>> {
    print!("{prompt}");
    io::stdout().flush()?;
    let mut buf = String::new();
    let n = io::stdin().lock().read_line(&mut buf)?;
    if n == 0 {
        return Ok(None);
    }
    Ok(Some(buf.trim().to_string()))
}

/// 首次运行时填充 5 条种子数据；已存在则静默跳过。
fn seed_users(conn: &Connection) {
    let seed = [
        ("alice", "pass_alice"),
        ("bob", "pass_bob"),
        ("carol", "pass_carol"),
        ("dave", "pass_dave"),
        ("eve", "pass_eve"),
    ];
    for (u, p) in seed {
        let _ = register_user(conn, u, p);
    }
}

fn action_register(conn: &Connection) -> Result<()> {
    let username = match read_line("用户名: ")? {
        Some(s) if !s.is_empty() => s,
        _ => {
            println!("用户名不能为空。");
            return Ok(());
        }
    };
    let password = match read_line("密码:   ")? {
        Some(s) if !s.is_empty() => s,
        _ => {
            println!("密码不能为空。");
            return Ok(());
        }
    };
    match register_user(conn, &username, &password) {
        Ok(id) => println!("✓ 注册成功: id={id}, username={username}"),
        Err(e) => println!("✗ 注册失败: {e}"),
    }
    Ok(())
}

fn action_query(conn: &Connection) -> Result<()> {
    let username = match read_line("要查询的用户名: ")? {
        Some(s) if !s.is_empty() => s,
        _ => {
            println!("用户名不能为空。");
            return Ok(());
        }
    };
    match find_user_by_username(conn, &username)? {
        Some(u) => {
            println!("✓ 找到用户:");
            println!("  id         = {}", u.id);
            println!("  username   = {}", u.username);
            println!("  password   = {}", u.password);
            println!("  created_at = {}", u.created_at.format("%Y-%m-%d %H:%M:%S"));
        }
        None => println!("✗ 未找到用户: {username}"),
    }
    Ok(())
}

fn main() -> Result<()> {
    let conn = init_db("users.db")?;
    seed_users(&conn);

    println!("=== 用户注册系统 (SQLite) ===");
    loop {
        println!();
        println!("[1] 注册新用户");
        println!("[2] 按用户名查询");
        println!("[q] 退出");
        let choice = match read_line("> ")? {
            Some(s) => s,
            None => {
                println!("\n再见。");
                break;
            }
        };
        match choice.as_str() {
            "1" => action_register(&conn)?,
            "2" => action_query(&conn)?,
            "q" | "Q" | "quit" | "exit" => {
                println!("再见。");
                break;
            }
            "" => continue,
            other => println!("无效选项: {other}"),
        }
    }
    Ok(())
}
