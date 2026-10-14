// 任务三：商品库存管理（PostgreSQL）
// 要求：
//   1. 在 PostgreSQL 中创建 products 表
//   2. 字段：商品 ID、名称、库存数量、最后更新时间
//   3. 实现库存扣减功能（购买商品后更新库存）
//   4. 编写查询库存量低于 10 件的商品功能

use std::io::{self, BufRead, Write};
use anyhow::{anyhow, Result};
use chrono::{DateTime, Local};
use postgres::{Client, NoTls};

const DB_URL: &str = "host=127.0.0.1 port=5432 user=lab3 password=lab3pass dbname=lab3";

#[derive(Debug)]
#[allow(dead_code)]
struct Product {
    id: i32,
    name: String,
    stock: i32,
    updated_at: DateTime<Local>,
}

fn connect() -> Result<Client> {
    Ok(Client::connect(DB_URL, NoTls)?)
}

/// 建表 + 插入若干样例数据（如果表为空）。
fn init_schema(client: &mut Client) -> Result<()> {
    client.batch_execute(
        "CREATE TABLE IF NOT EXISTS products (
            id         SERIAL PRIMARY KEY,
            name       TEXT NOT NULL,
            stock      INTEGER NOT NULL,
            updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        );
        INSERT INTO products (name, stock)
        SELECT * FROM (VALUES
            ('面包', 25),
            ('牛奶', 8),
            ('鸡蛋', 30),
            ('黄油', 5),
            ('酸奶', 3)
        ) AS seed(name, stock)
        WHERE NOT EXISTS (SELECT 1 FROM products);",
    )?;
    Ok(())
}

/// 扣减库存：qty > 0；当前库存不足时返回错误，不能扣成负数。
fn decrement_stock(client: &mut Client, product_id: i32, qty: i32) -> Result<i32> {
    if qty <= 0 {
        return Err(anyhow!("扣减数量必须为正"));
    }
    let row = client.query_opt(
        "UPDATE products
         SET stock = stock - $1, updated_at = NOW()
         WHERE id = $2 AND stock >= $1
         RETURNING stock",
        &[&qty, &product_id],
    )?;

    match row {
        Some(r) => Ok(r.get::<_, i32>(0)),
        None => Err(anyhow!("商品 {} 不存在或库存不足", product_id)),
    }
}

/// 查询库存低于 10 件的商品。
fn list_low_stock(client: &mut Client, threshold: i32) -> Result<Vec<Product>> {
    // TODO:
    //   SELECT id, name, stock, updated_at FROM products
    //   WHERE stock < $1
    //   ORDER BY stock ASC;
    //   使用 client.query(sql, &[&threshold])? 遍历构造 Product。
    let rows = client.query(
        "SELECT id, name, stock, updated_at FROM products
         WHERE stock < $1
         ORDER BY stock ASC",
        &[&threshold],
    )?;

    let products = rows
        .iter()
        .map(|r| Product {
            id: r.get("id"),
            name: r.get("name"),
            stock: r.get("stock"),
            updated_at: r.get("updated_at"),
        })
        .collect();
    Ok(products)
}

/// 查询所有商品。
fn list_all(client: &mut Client) -> Result<Vec<Product>> {
    let rows = client.query(
        "SELECT id, name, stock, updated_at FROM products ORDER BY id",
        &[],
    )?;
    Ok(rows
        .iter()
        .map(|r| Product {
            id: r.get("id"),
            name: r.get("name"),
            stock: r.get("stock"),
            updated_at: r.get("updated_at"),
        })
        .collect())
}

/// 美观打印商品列表。
fn print_products(title: &str, items: &[Product]) {
    println!("{title}（共 {} 件）：", items.len());
    for p in items {
        println!(
            "  id={:<3} 名称={:<6} 库存={:<5} 更新于 {}",
            p.id,
            p.name,
            p.stock,
            p.updated_at.format("%Y-%m-%d %H:%M:%S")
        );
    }
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

fn action_buy(client: &mut Client) -> Result<()> {
    let id: i32 = match read_line("商品 id: ")?.as_deref() {
        Some(s) if !s.is_empty() => match s.parse() {
            Ok(n) => n,
            Err(_) => {
                println!("商品 id 必须是整数。");
                return Ok(());
            }
        },
        _ => {
            println!("商品 id 不能为空。");
            return Ok(());
        }
    };
    let qty: i32 = match read_line("购买数量: ")?.as_deref() {
        Some(s) if !s.is_empty() => match s.parse() {
            Ok(n) => n,
            Err(_) => {
                println!("数量必须是整数。");
                return Ok(());
            }
        },
        _ => {
            println!("数量不能为空。");
            return Ok(());
        }
    };

    match decrement_stock(client, id, qty) {
        Ok(remain) => println!("✓ 购买成功：商品 {id} 剩余库存 = {remain}"),
        Err(e) => {
            println!("✗ 购买失败：{e}");
            return Ok(());
        }
    }

    let low = list_low_stock(client, 10)?;
    print_products("库存低于 10 件的商品", &low);
    Ok(())
}

fn main() -> Result<()> {
    let mut client = connect()?;
    init_schema(&mut client)?;

    println!("=== 商品库存管理 (PostgreSQL) ===");
    loop {
        println!();
        println!("[1] 查看所有商品");
        println!("[2] 购买商品");
        println!("[q] 退出");
        let choice = match read_line("> ")? {
            Some(s) => s,
            None => {
                println!("\n再见。");
                break;
            }
        };
        match choice.as_str() {
            "1" => {
                let all = list_all(&mut client)?;
                print_products("所有商品", &all);
            }
            "2" => action_buy(&mut client)?,
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
